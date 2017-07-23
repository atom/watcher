#include <string>
#include <memory>

#include "../worker_platform.h"
#include "../worker_thread.h"
#include "../../message.h"
#include "../../log.h"

using std::string;
using std::unique_ptr;

class LinuxWorkerPlatform : public WorkerPlatform {
public:
  LinuxWorkerPlatform(WorkerThread *thread) : WorkerPlatform(thread) {};

  void wake() override
  {
    //
  }

  void listen() override
  {
    //
  }

  void handle_add_command(const ChannelID channel, const string &root_path)
  {
    //
  }

  void handle_remove_command(const ChannelID channel)
  {
    //
  }
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new LinuxWorkerPlatform(thread));
}
