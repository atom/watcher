#include <string>
#include <memory>

#include "../worker_platform.h"
#include "../worker_thread.h"
#include "../../message.h"
#include "../../log.h"

using std::string;
using std::unique_ptr;

class WindowsWorkerPlatform : public WorkerPlatform {
public:
  WindowsWorkerPlatform(WorkerThread *thread) : WorkerPlatform(thread) {};

  Result<> wake() override
  {
    return ok_result();
  }

  Result<> listen() override
  {
    return ok_result();
  }

  Result<> handle_add_command(const ChannelID channel, const string &root_path)
  {
    return ok_result();
  }

  Result<> handle_remove_command(const ChannelID channel)
  {
    return ok_result();
  }
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new WindowsWorkerPlatform(thread));
}
