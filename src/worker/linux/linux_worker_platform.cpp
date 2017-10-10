#include <memory>
#include <poll.h>
#include <string>

#include "../../helper/linux/helper.h"
#include "../../log.h"
#include "../../message.h"
#include "../../result.h"
#include "../worker_platform.h"
#include "../worker_thread.h"
#include "cookie_jar.h"
#include "pipe.h"
#include "side_effect.h"
#include "watch_registry.h"

using std::string;
using std::unique_ptr;

// Platform-specific worker implementation for Linux systems.
class LinuxWorkerPlatform : public WorkerPlatform
{
public:
  LinuxWorkerPlatform(WorkerThread *thread) :
    WorkerPlatform(thread),
    pipe("worker pipe"){
      //
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
      int result = poll(to_poll, 2, -1);

      if (result < 0) {
        return errno_result<>("Unable to poll");
      } else if (result == 0) {
        return error_result("Unexpected poll() timeout");
      }

      if (to_poll[0].revents & (POLLIN | POLLERR)) {
        Result<> cr = pipe.consume();
        if (cr.is_error()) return cr;

        Result<> hr = handle_commands();
        if (hr.is_error()) return hr;
      }

      if (to_poll[1].revents & (POLLIN | POLLERR)) {
        MessageBuffer messages;
        SideEffect side;

        Result<> r = registry.consume(messages, jar, side);

        if (!messages.empty()) {
          r &= emit_all(messages.begin(), messages.end());
        }
        r &= side.enact_in(&registry);

        if (r.is_error()) return r;
      }
    }

    return error_result("Polling loop exited unexpectedly");
  }

  // Recursively watch a directory tree.
  Result<bool> handle_add_command(CommandID command, ChannelID channel, const string &root_path) override
  {
    return registry.add(channel, string(root_path), true).propagate(true);
  }

  // Unwatch a directory tree.
  Result<bool> handle_remove_command(CommandID command, ChannelID channel) override
  {
    return registry.remove(channel).propagate(true);
  }

private:
  Pipe pipe;
  WatchRegistry registry;
  CookieJar jar;
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new LinuxWorkerPlatform(thread));
}
