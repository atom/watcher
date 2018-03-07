#include <iostream>
#include <memory>
#include <poll.h>
#include <string>
#include <vector>

#include "../../helper/linux/helper.h"
#include "../../log.h"
#include "../../message.h"
#include "../../result.h"
#include "../recent_file_cache.h"
#include "../worker_platform.h"
#include "../worker_thread.h"
#include "cookie_jar.h"
#include "pipe.h"
#include "side_effect.h"
#include "watch_registry.h"

using std::endl;
using std::ostream;
using std::string;
using std::unique_ptr;
using std::vector;

const size_t DEFAULT_CACHE_SIZE = 4096;

// In milliseconds
const int RENAME_TIMEOUT = 500;

// Platform-specific worker implementation for Linux systems.
class LinuxWorkerPlatform : public WorkerPlatform
{
public:
  LinuxWorkerPlatform(WorkerThread *thread) : WorkerPlatform(thread), cache{DEFAULT_CACHE_SIZE}
  {
    report_errable(pipe);
    report_errable(registry);
    freeze();
  };

  // Inform the listen() loop that one or more commands are waiting from the main thread.
  Result<> wake() override { return pipe.signal(); }

  // Main event loop. Use poll(2) to wait on I/O from either the Pipe or inotify events.
  Result<> listen() override
  {
    pollfd to_poll[2];
    to_poll[0].fd = pipe.get_read_fd();
    to_poll[0].events = POLLIN;
    to_poll[0].revents = 0;
    to_poll[1].fd = registry.get_read_fd();
    to_poll[1].events = POLLIN;
    to_poll[1].revents = 0;

    while (true) {
      int result = poll(to_poll, 2, RENAME_TIMEOUT);

      if (result < 0) {
        return errno_result<>("Unable to poll");
      }

      if (result == 0) {
        // Poll timeout. Cycle the CookieJar.
        MessageBuffer messages;
        jar.flush_oldest_batch(messages, cache);

        if (!messages.empty()) {
          LOGGER << "Flushing " << plural(messages.size(), "unpaired rename") << "." << endl;
          Result<> er = emit_all(messages.begin(), messages.end());
          if (er.is_error()) return er;
        }

        continue;
      }

      if ((to_poll[0].revents & (POLLIN | POLLERR)) != 0u) {
        Result<> cr = pipe.consume();
        if (cr.is_error()) return cr;

        Result<> hr = handle_commands();
        if (hr.is_error()) return hr;
      }

      if ((to_poll[1].revents & (POLLIN | POLLERR)) != 0u) {
        MessageBuffer messages;

        Result<> cr = registry.consume(messages, jar, cache);
        if (cr.is_error()) LOGGER << cr << endl;

        if (!messages.empty()) {
          Result<> er = emit_all(messages.begin(), messages.end());
          if (er.is_error()) return er;
        }
      }
    }

    return error_result("Polling loop exited unexpectedly");
  }

  // Recursively watch a directory tree.
  Result<bool> handle_add_command(CommandID /*command*/,
    ChannelID channel,
    const string &root_path,
    bool recursive) override
  {
    Timer t;
    vector<string> poll;

    ostream &logline = LOGGER << "Adding watcher for path " << root_path;
    if (!recursive) {
      logline << " (non-recursively)";
    }
    logline << " at channel " << channel << "." << endl;

    Result<> r = registry.add(channel, string(root_path), recursive, poll);
    if (r.is_error()) return r.propagate<bool>();

    if (!poll.empty()) {
      vector<Message> poll_messages;
      poll_messages.reserve(poll.size());

      for (string &poll_root : poll) {
        poll_messages.emplace_back(
          CommandPayloadBuilder::add(channel, move(poll_root), recursive, poll.size()).build());
      }

      t.stop();
      LOGGER << "Watcher for path " << root_path << " and " << plural(poll.size(), "polled watch root") << " added in "
             << t << "." << endl;
      return emit_all(poll_messages.begin(), poll_messages.end()).propagate(false);
    }

    t.stop();
    LOGGER << "Watcher for path " << root_path << " added in " << t << "." << endl;
    return ok_result(true);
  }

  // Unwatch a directory tree.
  Result<bool> handle_remove_command(CommandID /*command*/, ChannelID channel) override
  {
    return registry.remove(channel).propagate(true);
  }

private:
  Pipe pipe;
  WatchRegistry registry;
  CookieJar jar;
  RecentFileCache cache;
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new LinuxWorkerPlatform(thread));
}
