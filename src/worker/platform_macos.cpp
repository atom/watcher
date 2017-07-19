#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <chrono>
#include <utility>
#include <iomanip>
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#include <errno.h>

#include "platform.h"
#include "worker_thread.h"
#include "../log.h"
#include "../message.h"

using std::vector;
using std::string;
using std::endl;
using std::unique_ptr;
using std::move;
using std::pair;
using std::make_pair;
using std::unordered_set;
using std::unordered_map;
using std::multimap;
using std::dec;
using std::hex;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::seconds;

static const CFAbsoluteTime LATENCY = 0;

static const FSEventStreamEventFlags CREATE_FLAGS = kFSEventStreamEventFlagItemCreated;

static const FSEventStreamEventFlags DELETED_FLAGS = kFSEventStreamEventFlagItemRemoved;

static const FSEventStreamEventFlags MODIFY_FLAGS = kFSEventStreamEventFlagItemInodeMetaMod |
  kFSEventStreamEventFlagItemFinderInfoMod |
  kFSEventStreamEventFlagItemChangeOwner |
  kFSEventStreamEventFlagItemXattrMod |
  kFSEventStreamEventFlagItemModified;

static const FSEventStreamEventFlags RENAME_FLAGS = kFSEventStreamEventFlagItemRenamed;

static const FSEventStreamEventFlags IS_FILE = kFSEventStreamEventFlagItemIsFile;

static const FSEventStreamEventFlags IS_DIRECTORY = kFSEventStreamEventFlagItemIsDir;

static void command_perform_helper(void *info);

static void event_stream_helper(
  ConstFSEventStreamRef event_stream,
  void *info,
  size_t num_events,
  void *event_paths,
  const FSEventStreamEventFlags *event_flags,
  const FSEventStreamEventId *event_ids);

struct Subscription {
  WorkerPlatform *platform;
  ChannelID channel;
  FSEventStreamRef event_stream;

  Subscription(WorkerPlatform *platform, ChannelID channel) : platform{platform}, channel{channel}, event_stream{NULL}
  {
    //
  }
};

class RecentFileCache {
public:
  void does_exist(const string &path)
  {
    bool inserted = seen_paths.insert(path).second;
    if (inserted) {
      time_point<steady_clock> ts = steady_clock::now();
      by_timestamp.insert({ts, path});
    }
  }

  void does_not_exist(const string &path)
  {
    seen_paths.erase(path);
  }

  bool has_been_seen(const string &path)
  {
    return seen_paths.find(path) != seen_paths.end();
  }

  void purge()
  {
    time_point<steady_clock> oldest = steady_clock::now() - seconds(5);

    auto to_keep = by_timestamp.upper_bound(oldest);
    for (auto it = by_timestamp.begin(); it != to_keep && it != by_timestamp.end(); ++it) {
      seen_paths.erase(it->second);
    }

    if (to_keep != by_timestamp.begin()) {
      by_timestamp.erase(by_timestamp.begin(), to_keep);
    }
  }

private:
  unordered_set<string> seen_paths;
  multimap<time_point<steady_clock>, string> by_timestamp;
};

class MacOSWorkerPlatform : public WorkerPlatform {
public:
  MacOSWorkerPlatform(WorkerThread *thread) :
    WorkerPlatform(thread),
    run_loop{nullptr},
    command_source{nullptr}
  {
    //
  };

  ~MacOSWorkerPlatform() override
  {
    if (command_source) CFRelease(command_source);
    if (run_loop) CFRelease(run_loop);
  }

  void wake() override
  {
    CFRunLoopSourceSignal(command_source);
    CFRunLoopWakeUp(run_loop);
  }

  void listen() override
  {
    run_loop = CFRunLoopGetCurrent();
    CFRetain(run_loop);

    CFRunLoopSourceContext command_context = {
      0, // version
      this, // info
      NULL, // retain
      NULL, // release
      NULL, // copyDescription
      NULL, // equal
      NULL, // hash
      NULL, // schedule
      NULL, // cancel
      command_perform_helper // perform
    };
    command_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &command_context);
    CFRunLoopAddSource(run_loop, command_source, kCFRunLoopDefaultMode);

    CFRunLoopRun();
    LOGGER << "Run loop ended unexpectedly." << endl;
  }

  void handle_add_command(const ChannelID channel, const string &root_path) override
  {
    LOGGER << "Adding watcher for path " << root_path << " at channel " << channel << "." << endl;

    Subscription *subscription = new Subscription(this, channel);

    FSEventStreamContext stream_context = {
      0, // version
      subscription, // info
      NULL, // retain
      NULL, // release
      NULL // copyDescription
    };

    CFStringRef watch_root = CFStringCreateWithBytes(
      kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(root_path.c_str()),
      root_path.size(),
      kCFStringEncodingUTF8,
      false
    );
    if (watch_root == NULL) {
      // TODO report error back in Ack
      return;
    }

    CFArrayRef watch_roots = CFArrayCreate(
      kCFAllocatorDefault,
      reinterpret_cast<const void**>(&watch_root),
      1,
      NULL
    );
    if (watch_roots == NULL) {
      // TODO report error back in Ack
      CFRelease(watch_root);
      return;
    }

    FSEventStreamRef event_stream = FSEventStreamCreate(
      kCFAllocatorDefault,
      event_stream_helper,
      &stream_context,
      watch_roots,
      kFSEventStreamEventIdSinceNow,
      LATENCY,
      kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents
    );
    subscription->event_stream = event_stream;

    subscriptions.insert(make_pair(channel, subscription));

    FSEventStreamScheduleWithRunLoop(event_stream, run_loop, kCFRunLoopDefaultMode);
    if (!FSEventStreamStart(event_stream)) {
      // TODO Report error back in Ack
    }

    CFRelease(watch_roots);
    CFRelease(watch_root);
  }

  void handle_remove_command(const ChannelID channel) override
  {
    LOGGER << "Removing watcher for channel " << channel << "." << endl;

    auto maybe_subscription = subscriptions.find(channel);
    if (maybe_subscription == subscriptions.end()) {
      LOGGER << "No subscription for channel " << channel << "." << endl;
      return;
    }

    Subscription *subscription = maybe_subscription->second;
    subscriptions.erase(maybe_subscription);

    FSEventStreamStop(subscription->event_stream);
    FSEventStreamInvalidate(subscription->event_stream);
    FSEventStreamRelease(subscription->event_stream);

    delete subscription;
  }

  void handle_fs_event(
    ChannelID channel,
    ConstFSEventStreamRef event_stream,
    size_t num_events,
    void *event_paths,
    const FSEventStreamEventFlags *event_flags,
    const FSEventStreamEventId *event_ids)
  {
    char **paths = reinterpret_cast<char**>(event_paths);
    vector<Message> messages;
    string rename_old_path;

    LOGGER << "Filesystem event batch of size " << num_events << " received." << endl;
    messages.reserve(num_events);

    for (size_t i = 0; i < num_events; i++) {
      bool created = (event_flags[i] & CREATE_FLAGS) != 0;
      bool deleted = (event_flags[i] & DELETED_FLAGS) != 0;
      bool modified = (event_flags[i] & MODIFY_FLAGS) != 0;
      bool renamed = (event_flags[i] & RENAME_FLAGS) != 0;

      bool is_file = (event_flags[i] & IS_FILE) != 0;
      bool is_directory = (event_flags[i] & IS_DIRECTORY) != 0;

      string event_path(paths[i]);
      bool stat_performed = false;
      int stat_errno = 0;

      struct stat path_stat;

      LOGGER << "Received event: "
        << event_ids[i] << " => "
        << (is_directory ? "directory" : "file")
        << " at [" << event_path << "]"
        << " created=" << created
        << " deleted=" << deleted
        << " modified=" << modified
        << " renamed=" << renamed
        << " flags=" << hex << event_flags[i] << dec
        << endl;

      EntryKind kind;
      if (is_file && !is_directory) {
        kind = KIND_FILE;
      } else if (is_directory && !is_file) {
        kind = KIND_DIRECTORY;
      } else {
        // FIXME will file and directory events ever be coalesced?
        // FIXME handle symlinks, named pipes, etc
        LOGGER << "Unknown or ambiguous filesystem entry kind from event flags "
          << hex << event_flags[i] << dec << "." << endl;

        if (lstat(event_path.c_str(), &path_stat) != 0) {
          stat_errno = errno;
        }
        stat_performed = true;
        // FIXME handle other stat errors

        if ((path_stat.st_mode & S_IFDIR) != 0) {
          kind = KIND_DIRECTORY;
        } else if ((path_stat.st_mode & S_IFREG) != 0) {
          kind = KIND_FILE;
        } else {
          LOGGER << "Unhandled stat() flags " << hex << path_stat.st_mode << "." << endl;
          continue;
        }
      }

      // Uncoalesed, single-event, non-rename flags.

      if (created && !(deleted || modified || renamed)) {
        cache.does_exist(event_path);
        enqueue_creation(messages, channel, kind, event_path);
        continue;
      }

      if (deleted && !(created || modified || renamed)) {
        cache.does_not_exist(event_path);
        enqueue_deletion(messages, channel, kind, event_path);
        continue;
      }

      if (modified && !(created || deleted || renamed)) {
        cache.does_exist(event_path);
        enqueue_modification(messages, channel, kind, event_path);
        continue;
      }

      // Call lstat() to note the final state of the entry.
      if (!stat_performed) {
        if (lstat(event_path.c_str(), &path_stat) != 0) {
          stat_errno = errno;
        }
        stat_performed = true;
      }

      if (stat_errno == ENOENT) {
        // The entry no longer exists. If this is a rename event, note this as the old name of the renamed file and
        // wait for the other half of the notification to arrive. Otherwise, emit a creation event if it has never been
        // seen before, then emit a deletion event.

        if (renamed) {
          rename_old_path = event_path;
        } else {
          if (!cache.has_been_seen(event_path)) {
            enqueue_creation(messages, channel, kind, event_path);
          }
          enqueue_deletion(messages, channel, kind, event_path);
        }
        continue;
      }

      // The entry currently exists. Use the event flags to determine the sequence of actions performed on that path.

      bool seen_before = cache.has_been_seen(event_path);
      cache.does_exist(event_path);

      if (seen_before) {
        // This is *not* the first time an event at this path has been seen.
        if (renamed && !rename_old_path.empty()) {
          // The existing file must have been deleted and a new file renamed in its place.
          enqueue_deletion(messages, channel, kind, event_path);
          enqueue_rename(messages, channel, kind, rename_old_path, event_path);

          rename_old_path.clear();
        } else if (deleted) {
          // Rapid creation and deletion. There may be a lost modification event just before deletion or just after
          // recreation.
          enqueue_deletion(messages, channel, kind, event_path);
          enqueue_creation(messages, channel, kind, event_path);
        } else {
          // Modification of an existing file.
          enqueue_modification(messages, channel, kind, event_path);
        }
      } else {
        // This *is* the first time an event has been seen at this path.
        if (renamed && !rename_old_path.empty()) {
          // The other half of an existing rename.
          enqueue_rename(messages, channel, kind, rename_old_path, event_path);

          rename_old_path.clear();
        } else if (deleted) {
          // The only way for the deletion flag to be set on a file we haven't seen before is for the file to
          // be rapidly created, deleted, and created again.
          enqueue_creation(messages, channel, kind, event_path);
          enqueue_deletion(messages, channel, kind, event_path);
          enqueue_creation(messages, channel, kind, event_path);
        } else {
          // Otherwise, it must have been created. This may conceal a separate modification event just after
          // the file's creation.
          enqueue_creation(messages, channel, kind, event_path);
        }
      }
    }

    emit_all(messages.begin(), messages.end());

    LOGGER << "Filesystem event batch of size " << num_events << " completed. "
      << messages.size() << " message(s) produced." << endl;

    cache.purge();
  }

private:
  void enqueue_creation(
    vector<Message> &messages,
    const ChannelID &channel,
    const EntryKind &kind,
    string created_path)
  {
    string empty_path;

    FileSystemPayload payload(channel, ACTION_CREATED, kind, move(created_path), move(empty_path));
    Message event_message(move(payload));

    LOGGER << "Emitting filesystem message " << event_message << endl;

    messages.push_back(move(event_message));
  }

  void enqueue_modification(
    vector<Message> &messages,
    const ChannelID &channel,
    const EntryKind &kind,
    string modified_path)
  {
    string empty_path;

    FileSystemPayload payload(channel, ACTION_MODIFIED, kind, move(modified_path), move(empty_path));
    Message event_message(move(payload));

    LOGGER << "Emitting filesystem message " << event_message << endl;

    messages.push_back(move(event_message));
  }

  void enqueue_deletion(
    vector<Message> &messages,
    const ChannelID &channel,
    const EntryKind &kind,
    string deleted_path)
  {
    string empty_path;

    FileSystemPayload payload(channel, ACTION_DELETED, kind, move(deleted_path), move(empty_path));
    Message event_message(move(payload));

    LOGGER << "Emitting filesystem message " << event_message << endl;

    messages.push_back(move(event_message));
  }

  void enqueue_rename(
    vector<Message> &messages,
    const ChannelID &channel,
    const EntryKind &kind,
    string old_path,
    string new_path)
  {
    FileSystemPayload payload(channel, ACTION_RENAMED, kind, move(old_path), move(new_path));
    Message event_message(move(payload));

    LOGGER << "Emitting filesystem message " << event_message << endl;

    messages.push_back(move(event_message));
  }

  CFRunLoopRef run_loop;
  CFRunLoopSourceRef command_source;

  unordered_map<ChannelID, Subscription*> subscriptions;
  RecentFileCache cache;
};

static void command_perform_helper(void *info)
{
  MacOSWorkerPlatform *platform = reinterpret_cast<MacOSWorkerPlatform*>(info);
  platform->handle_commands();
}

static void event_stream_helper(
  ConstFSEventStreamRef event_stream,
  void *info,
  size_t num_events,
  void *event_paths,
  const FSEventStreamEventFlags *event_flags,
  const FSEventStreamEventId *event_ids)
{
  Subscription *sub = reinterpret_cast<Subscription*>(info);
  MacOSWorkerPlatform *platform = static_cast<MacOSWorkerPlatform*>(sub->platform);

  platform->handle_fs_event(sub->channel, event_stream, num_events, event_paths, event_flags, event_ids);
}

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new MacOSWorkerPlatform(thread));
}
