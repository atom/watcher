#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <utility>
#include <iomanip>
#include <sstream>
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#include <errno.h>

#include "../worker_platform.h"
#include "../worker_thread.h"
#include "recent_file_cache.h"
#include "event_handler.h"
#include "flags.h"
#include "../../log.h"
#include "../../message.h"
#include "../../result.h"

using std::vector;
using std::ostream;
using std::string;
using std::ostringstream;
using std::endl;
using std::unique_ptr;
using std::move;
using std::pair;
using std::make_pair;
using std::unordered_set;
using std::unordered_map;
using std::dec;
using std::hex;

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

  Result<> wake() override
  {
    CFRunLoopSourceSignal(command_source);
    CFRunLoopWakeUp(run_loop);

    return ok_result();
  }

  Result<> listen() override
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
    return ok_result();
  }

  Result<> handle_add_command(const ChannelID channel, const string &root_path) override
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
      string msg("Unable to allocate string for root path: ");
      msg += root_path;
      return error_result(move(msg));
    }

    CFArrayRef watch_roots = CFArrayCreate(
      kCFAllocatorDefault,
      reinterpret_cast<const void**>(&watch_root),
      1,
      NULL
    );
    if (watch_roots == NULL) {
      string msg("Unable to allocate array for watch root: ");
      msg += root_path;
      CFRelease(watch_root);

      return error_result(move(msg));
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
      string msg("Unable start event stream for watch root: ");
      msg += root_path;

      CFRelease(watch_roots);
      CFRelease(watch_root);
      FSEventStreamRelease(event_stream);
      return error_result(move(msg));
    }

    CFRelease(watch_roots);
    CFRelease(watch_root);
    return ok_result();
  }

  Result<> handle_remove_command(const ChannelID channel) override
  {
    LOGGER << "Removing watcher for channel " << channel << "." << endl;

    auto maybe_subscription = subscriptions.find(channel);
    if (maybe_subscription == subscriptions.end()) {
      LOGGER << "No subscription for channel " << channel << "." << endl;
      return ok_result();
    }

    Subscription *subscription = maybe_subscription->second;
    subscriptions.erase(maybe_subscription);

    FSEventStreamStop(subscription->event_stream);
    FSEventStreamInvalidate(subscription->event_stream);
    FSEventStreamRelease(subscription->event_stream);

    delete subscription;
    return ok_result();
  }

  void handle_fs_event(
    ChannelID channel_id,
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

    EventHandler handler(messages, cache, channel_id);
    for (size_t i = 0; i < num_events; i++) {
      string event_path(paths[i]);
      handler.handle(event_path, event_flags[i]);
    }
    handler.flush();

    Result<> er = emit_all(messages.begin(), messages.end());
    if (er.is_error()) {
      LOGGER << "Unable to emit ack messages: " << er << "." << endl;
      return;
    }

    LOGGER << "Filesystem event batch of size " << num_events << " completed. "
      << messages.size() << " message(s) produced." << endl;

    cache.prune();
  }

private:
  CFRunLoopRef run_loop;
  CFRunLoopSourceRef command_source;

  unordered_map<ChannelID, Subscription*> subscriptions;
  RecentFileCache cache;
};

static void command_perform_helper(void *info)
{
  MacOSWorkerPlatform *platform = reinterpret_cast<MacOSWorkerPlatform*>(info);
  Result<> r = platform->handle_commands();
  if (r.is_error()) {
    LOGGER << "Unable to handle incoming commands: " << r << "." << endl;
  }
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
