#include <CoreServices/CoreServices.h>
#include <cerrno>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <utility>

#include "../../log.h"
#include "../../message.h"
#include "../../message_buffer.h"
#include "../../result.h"
#include "../worker_platform.h"
#include "../worker_thread.h"
#include "event_handler.h"
#include "flags.h"
#include "recent_file_cache.h"

using std::endl;
using std::make_pair;
using std::move;
using std::ostream;
using std::ostringstream;
using std::string;
using std::unique_ptr;
using std::unordered_map;

class MacOSWorkerPlatform;

static void command_perform_helper(void *info);

static void event_stream_helper(ConstFSEventStreamRef event_stream,
  void *info,
  size_t num_events,
  void *event_paths,
  const FSEventStreamEventFlags *event_flags,
  const FSEventStreamEventId *event_ids);

struct Subscription
{
  MacOSWorkerPlatform *platform;
  ChannelID channel;
  FSEventStreamRef event_stream;

  Subscription(MacOSWorkerPlatform *platform, ChannelID channel) :
    platform{platform},
    channel{channel},
    event_stream{nullptr}
  {
    //
  }
};

class MacOSWorkerPlatform : public WorkerPlatform
{
public:
  MacOSWorkerPlatform(WorkerThread *thread) :
    WorkerPlatform(thread),
    run_loop{nullptr},
    command_source{nullptr} {
      //
    };

  ~MacOSWorkerPlatform() override
  {
    if (command_source != nullptr) CFRelease(command_source);
    if (run_loop != nullptr) CFRelease(run_loop);
  }

  MacOSWorkerPlatform(const MacOSWorkerPlatform &) = delete;
  MacOSWorkerPlatform(MacOSWorkerPlatform &&) = delete;
  MacOSWorkerPlatform &operator=(const MacOSWorkerPlatform &) = delete;
  MacOSWorkerPlatform &operator=(MacOSWorkerPlatform &&) = delete;

  Result<> wake() override
  {
    if (!is_healthy()) return health_err_result();

    CFRunLoopSourceSignal(command_source);
    CFRunLoopWakeUp(run_loop);

    return ok_result();
  }

  Result<> listen() override
  {
    if (!is_healthy()) return health_err_result();

    run_loop = CFRunLoopGetCurrent();
    CFRetain(run_loop);

    CFRunLoopSourceContext command_context = {
      0,  // version
      this,  // info
      nullptr,  // retain
      nullptr,  // release
      nullptr,  // copyDescription
      nullptr,  // equal
      nullptr,  // hash
      nullptr,  // schedule
      nullptr,  // cancel
      command_perform_helper  // perform
    };
    command_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &command_context);
    CFRunLoopAddSource(run_loop, command_source, kCFRunLoopDefaultMode);

    CFRunLoopRun();
    return ok_result();
  }

  Result<bool> handle_add_command(CommandID command, ChannelID channel, const string &root_path) override
  {
    if (!is_healthy()) return health_err_result().propagate<bool>();
    LOGGER << "Adding watcher for path " << root_path << " at channel " << channel << "." << endl;

    auto *subscription = new Subscription(this, channel);

    FSEventStreamContext stream_context = {
      0,  // version
      subscription,  // info
      nullptr,  // retain
      nullptr,  // release
      nullptr  // copyDescription
    };

    CFStringRef watch_root = CFStringCreateWithBytes(kCFAllocatorDefault,
      reinterpret_cast<const UInt8 *>(root_path.c_str()),
      root_path.size(),
      kCFStringEncodingUTF8,
      0u);
    if (watch_root == nullptr) {
      string msg("Unable to allocate string for root path: ");
      msg += root_path;
      return Result<bool>::make_error(move(msg));
    }

    CFArrayRef watch_roots =
      CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(&watch_root), 1, nullptr);
    if (watch_roots == nullptr) {
      string msg("Unable to allocate array for watch root: ");
      msg += root_path;
      CFRelease(watch_root);

      return Result<bool>::make_error(move(msg));
    }

    FSEventStreamRef event_stream = FSEventStreamCreate(kCFAllocatorDefault,
      event_stream_helper,
      &stream_context,
      watch_roots,
      kFSEventStreamEventIdSinceNow,
      LATENCY,
      kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents);
    if (event_stream == nullptr) {
      string msg("Unable to create event stream for watch root: ");
      msg += root_path;
      CFRelease(watch_roots);
      CFRelease(watch_root);

      return Result<bool>::make_error(move(msg));
    }

    subscription->event_stream = event_stream;
    subscriptions.insert(make_pair(channel, subscription));

    FSEventStreamScheduleWithRunLoop(event_stream, run_loop, kCFRunLoopDefaultMode);
    if (FSEventStreamStart(event_stream) == 0u) {
      LOGGER << "Falling back to polling for watch root " << root_path << "." << endl;

      CFRelease(watch_roots);
      CFRelease(watch_root);

      // Emit an Add command for the polling thread to pick up
      emit(Message(CommandPayload(COMMAND_ADD, command, string(root_path), channel)));
      return ok_result(false);
    }

    CFRelease(watch_roots);
    CFRelease(watch_root);

    cache.prepopulate(root_path, 4096);
    return ok_result(true);
  }

  Result<bool> handle_remove_command(CommandID /*command*/, ChannelID channel) override
  {
    if (!is_healthy()) return health_err_result().propagate<bool>();
    LOGGER << "Removing watcher for channel " << channel << "." << endl;

    auto maybe_subscription = subscriptions.find(channel);
    if (maybe_subscription == subscriptions.end()) {
      LOGGER << "No subscription for channel " << channel << "." << endl;
      return ok_result(true);
    }

    Subscription *subscription = maybe_subscription->second;
    subscriptions.erase(maybe_subscription);

    FSEventStreamStop(subscription->event_stream);
    FSEventStreamInvalidate(subscription->event_stream);
    FSEventStreamRelease(subscription->event_stream);

    delete subscription;
    return ok_result(true);
  }

  void handle_fs_event(ChannelID channel_id,
    ConstFSEventStreamRef /*event_stream*/,
    size_t num_events,
    void *event_paths,
    const FSEventStreamEventFlags *event_flags,
    const FSEventStreamEventId * /*event_ids*/)
  {
    auto **paths = reinterpret_cast<char **>(event_paths);
    MessageBuffer buffer;
    ChannelMessageBuffer message_buffer(buffer, channel_id);

    LOGGER << "Filesystem event batch of size " << num_events << " received." << endl;
    message_buffer.reserve(num_events);

    EventHandler handler(message_buffer, cache);
    for (size_t i = 0; i < num_events; i++) {
      string event_path(paths[i]);
      handler.handle(event_path, event_flags[i]);
    }
    handler.flush();

    Result<> er = emit_all(message_buffer.begin(), message_buffer.end());
    if (er.is_error()) {
      LOGGER << "Unable to emit ack messages: " << er << "." << endl;
      return;
    }

    LOGGER << "Filesystem event batch of size " << num_events << " completed. "
           << plural(message_buffer.size(), "message") << " produced." << endl;

    cache.prune();
  }

private:
  CFRunLoopRef run_loop;
  CFRunLoopSourceRef command_source;

  unordered_map<ChannelID, Subscription *> subscriptions;
  RecentFileCache cache;
};

static void command_perform_helper(void *info)
{
  auto *platform = reinterpret_cast<MacOSWorkerPlatform *>(info);
  Result<> r = platform->handle_commands();
  if (r.is_error()) {
    LOGGER << "Unable to handle incoming commands: " << r << "." << endl;
  }
}

static void event_stream_helper(ConstFSEventStreamRef event_stream,
  void *info,
  size_t num_events,
  void *event_paths,
  const FSEventStreamEventFlags *event_flags,
  const FSEventStreamEventId *event_ids)
{
  auto *sub = reinterpret_cast<Subscription *>(info);
  auto *platform = dynamic_cast<MacOSWorkerPlatform *>(sub->platform);

  platform->handle_fs_event(sub->channel, event_stream, num_events, event_paths, event_flags, event_ids);
}

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new MacOSWorkerPlatform(thread));
}
