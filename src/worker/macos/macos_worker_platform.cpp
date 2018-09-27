#include <CoreServices/CoreServices.h>
#include <cerrno>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <utility>

#include "../../helper/macos/helper.h"
#include "../../log.h"
#include "../../message.h"
#include "../../message_buffer.h"
#include "../../result.h"
#include "../recent_file_cache.h"
#include "../worker_platform.h"
#include "../worker_thread.h"
#include "batch_handler.h"
#include "flags.h"
#include "rename_buffer.h"
#include "subscription.h"

using std::bind;
using std::endl;
using std::move;
using std::ostream;
using std::ostringstream;
using std::set;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using std::placeholders::_5;

const size_t DEFAULT_CACHE_SIZE = 4096;

const size_t DEFAULT_CACHE_PREPOPULATION = 4096;

class MacOSWorkerPlatform : public WorkerPlatform
{
public:
  MacOSWorkerPlatform(WorkerThread *thread) : WorkerPlatform(thread), cache{DEFAULT_CACHE_SIZE} { freeze(); };

  ~MacOSWorkerPlatform() override = default;

  Result<> wake() override
  {
    if (command_source.empty() || run_loop.empty()) {
      return ok_result();
    }

    CFRunLoopSourceSignal(command_source.get());
    CFRunLoopWakeUp(run_loop.get());

    return ok_result();
  }

  Result<> init() override
  {
    run_loop.set_from_get(CFRunLoopGetCurrent());

    auto info = source_registry.create_info(bind(&MacOSWorkerPlatform::source_triggered, this));
    CFRunLoopSourceContext command_context{
      0,  // version
      static_cast<void *>(info.get()),  // info
      nullptr,  // retain
      nullptr,  // release
      nullptr,  // copyDescription
      nullptr,  // equal
      nullptr,  // hash
      nullptr,  // schedule
      nullptr,  // cancel
      SourceFnRegistry::callback  // perform
    };
    command_source.set_from_create(CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &command_context));
    CFRunLoopAddSource(run_loop.get(), command_source.get(), kCFRunLoopDefaultMode);

    static_cast<void>(info.release());
    return ok_result();
  }

  Result<> listen() override
  {
    CFRunLoopRun();
    return ok_result();
  }

  Result<bool> handle_add_command(CommandID command_id,
    ChannelID channel_id,
    const string &root_path,
    bool recursive) override
  {
    ostream &logline = LOGGER << "Adding watcher for path " << root_path;
    if (!recursive) {
      logline << " (non-recursively)";
    }
    logline << " at channel " << channel_id << "." << endl;

    auto info = event_stream_registry.create_info(
      bind(&MacOSWorkerPlatform::fs_event_triggered, this, channel_id, _1, _2, _3, _4, _5));
    FSEventStreamContext stream_context{
      0,  // version
      static_cast<void *>(info.get()),  // info
      nullptr,  // retain
      nullptr,  // release
      nullptr  // copyDescription
    };

    RefHolder<CFStringRef> watch_root(CFStringCreateWithBytes(kCFAllocatorDefault,
      reinterpret_cast<const UInt8 *>(root_path.c_str()),
      root_path.size(),
      kCFStringEncodingUTF8,
      0u));
    if (watch_root.empty()) {
      string msg("Unable to allocate string for root path: ");
      msg += root_path;
      return Result<bool>::make_error(move(msg));
    }

    RefHolder<CFArrayRef> watch_roots(
      CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(&watch_root), 1, nullptr));
    if (watch_roots.empty()) {
      string msg("Unable to allocate array for watch root: ");
      msg += root_path;
      return Result<bool>::make_error(move(msg));
    }

    RefHolder<FSEventStreamRef> event_stream(FSEventStreamCreate(kCFAllocatorDefault,  // allocator
      EventStreamFnRegistry::callback,  // callback
      &stream_context,  // context
      watch_roots.get(),  // paths_to_watch
      kFSEventStreamEventIdSinceNow,  // since_when
      LATENCY,  // latency
      kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents  // flags
      ));
    if (event_stream.empty()) {
      string msg("Unable to create event stream for watch root: ");
      msg += root_path;
      return Result<bool>::make_error(move(msg));
    }

    FSEventStreamScheduleWithRunLoop(event_stream.get(), run_loop.get(), kCFRunLoopDefaultMode);
    if (FSEventStreamStart(event_stream.get()) == 0u) {
      LOGGER << "Falling back to polling for watch root " << root_path << "." << endl;

      // Emit an Add command for the polling thread to pick up
      emit(Message(CommandPayloadBuilder::add(channel_id, string(root_path), true, 1).set_id(command_id).build()));
      return ok_result(false);
    }

    static_cast<void>(info.release());
    subscriptions.emplace(channel_id, Subscription(channel_id, recursive, string(root_path), move(event_stream)));

    cache.prepopulate(root_path, DEFAULT_CACHE_PREPOPULATION, recursive);
    return ok_result(true);
  }

  Result<bool> handle_remove_command(CommandID /*command_id*/, ChannelID channel_id) override
  {
    LOGGER << "Removing watcher for channel " << channel_id << "." << endl;

    auto maybe_sub = subscriptions.find(channel_id);
    if (maybe_sub == subscriptions.end()) {
      LOGGER << "No subscription for channel " << channel_id << "." << endl;
      return ok_result(true);
    }
    subscriptions.erase(maybe_sub);
    return ok_result(true);
  }

  void handle_cache_size_command(size_t cache_size) override
  {
    LOGGER << "Changing cache size to " << cache_size << "." << endl;
    cache.resize(cache_size);
  }

  void populate_status(Status &status) override
  {
    status.worker_subscription_count = subscriptions.size();
    status.worker_rename_buffer_size = rename_buffer.size();
    status.worker_recent_file_cache_size = cache.size();
  }

  FnRegistryAction source_triggered()
  {
    Result<> r = handle_commands();
    if (r.is_error()) LOGGER << "Unable to handle incoming commands: " << r << "." << endl;
    return FN_KEEP;
  }

  FnRegistryAction fs_event_triggered(ChannelID channel_id,
    ConstFSEventStreamRef /*ref*/,
    size_t num_events,
    void *event_paths,
    const FSEventStreamEventFlags *event_flags,
    const FSEventStreamEventId * /*event_ids*/)
  {
    auto **paths = reinterpret_cast<char **>(event_paths);
    MessageBuffer buffer;
    ChannelMessageBuffer message_buffer(buffer, channel_id);
    Timer t;

    LOGGER << "Filesystem event batch of size " << num_events << " received." << endl;
    auto sub = subscriptions.find(channel_id);
    if (sub == subscriptions.end()) {
      LOGGER << "No active subscription for channel " << channel_id << "." << endl;
      return FN_KEEP;
    }

    message_buffer.reserve(num_events);

    BatchHandler handler(message_buffer, cache, rename_buffer, sub->second.get_recursive(), sub->second.get_root());
    for (size_t i = 0; i < num_events; i++) {
      handler.event(string(paths[i]), event_flags[i]);
    }
    handler.handle_deferred();
    cache.apply();

    shared_ptr<set<RenameBuffer::Key>> out = rename_buffer.flush_unmatched(message_buffer, cache);
    if (!out->empty()) {
      LOGGER << "Scheduling expiration of " << out->size() << " unpaired rename entries on channel " << channel_id
             << "." << endl;
      CFAbsoluteTime fire_time = CFAbsoluteTimeGetCurrent() + RENAME_TIMEOUT;

      auto info = timer_registry.create_info(bind(&MacOSWorkerPlatform::timer_triggered, this, out, channel_id, _1));
      CFRunLoopTimerContext timer_context{
        0,  // version
        static_cast<void *>(info.get()),  // info
        nullptr,  // retain
        nullptr,  // release
        nullptr  // copy description
      };

      // timer is released in MacOSWorkerPlatform::timer_triggered.
      CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault,  // allocator
        fire_time,  // fire date
        0,  // interval (0 = oneshot)
        0,  // flags, ignored
        0,  // order, ignored
        TimerFnRegistry::callback,  // callout
        &timer_context  // context
      );

      CFRunLoopAddTimer(run_loop.get(), timer, kCFRunLoopDefaultMode);
      static_cast<void>(info.release());
    }

    Result<> er = emit_all(message_buffer.begin(), message_buffer.end());
    if (er.is_error()) {
      LOGGER << "Unable to emit filesystem event messages: " << er << "." << endl;
      return FN_KEEP;
    }
    t.stop();

    LOGGER << "Filesystem event batch of size " << num_events << " completed. "
           << plural(message_buffer.size(), "message") << " produced in " << t << "." << endl;
    cache.prune();

    return FN_KEEP;
  }

  FnRegistryAction timer_triggered(shared_ptr<set<RenameBuffer::Key>> keys,
    ChannelID channel_id,
    CFRunLoopTimerRef timer)
  {
    LOGGER << "Expiring " << plural(keys->size(), "rename entry", "rename entries") << " on channel " << channel_id
           << "." << endl;

    MessageBuffer buffer;
    ChannelMessageBuffer message_buffer(buffer, channel_id);

    shared_ptr<set<RenameBuffer::Key>> next = rename_buffer.flush_unmatched(message_buffer, cache, keys);
    assert(next->empty());
    keys.reset();

    Result<> er = emit_all(message_buffer.begin(), message_buffer.end());
    if (er.is_error()) LOGGER << "Unable to emit flushed rename event messages: " << er << "." << endl;

    CFRelease(timer);

    return FN_DISPOSE;
  }

  MacOSWorkerPlatform(const MacOSWorkerPlatform &) = delete;
  MacOSWorkerPlatform(MacOSWorkerPlatform &&) = delete;
  MacOSWorkerPlatform &operator=(const MacOSWorkerPlatform &) = delete;
  MacOSWorkerPlatform &operator=(MacOSWorkerPlatform &&) = delete;

private:
  SourceFnRegistry source_registry;
  TimerFnRegistry timer_registry;
  EventStreamFnRegistry event_stream_registry;

  unordered_map<ChannelID, Subscription> subscriptions;
  RenameBuffer rename_buffer;
  RecentFileCache cache;

  RefHolder<CFRunLoopSourceRef> command_source;
  RefHolder<CFRunLoopRef> run_loop;
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new MacOSWorkerPlatform(thread));
}
