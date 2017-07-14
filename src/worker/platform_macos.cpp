#include <memory>
#include <string>
#include <CoreServices/CoreServices.h>

#include "platform.h"
#include "worker_thread.h"
#include "../log.h"

using std::string;
using std::endl;
using std::unique_ptr;

static void command_perform_helper(void *info);

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

  void handle_add_command(const string &root_path) override
  {
    LOGGER << "Adding watcher for path " << root_path << "." << endl;
  }

  void handle_remove_command(const string &root_path) override
  {
    LOGGER << "Removing watcher for path " << root_path << "." << endl;
  }

private:
  CFRunLoopRef run_loop;
  CFRunLoopSourceRef command_source;
};

static void command_perform_helper(void *info)
{
  ((MacOSWorkerPlatform*) info)->handle_commands();
}

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new MacOSWorkerPlatform(thread));
}
