#include <string>
#include <memory>

#include "../worker_platform.h"
#include "../worker_thread.h"
#include "../../message.h"
#include "../../log.h"
#include "../../result.h"

using std::string;
using std::unique_ptr;

class LinuxWorkerPlatform : public WorkerPlatform {
public:
  LinuxWorkerPlatform(WorkerThread *thread) : WorkerPlatform(thread) {};

  Result<> wake() override
  {
    return ok_result();
  }

  Result<> listen() override
  {
    return ok_result();
  }

  Result<bool> handle_add_command(
    const CommandID command,
    const ChannelID channel,
    const string &root_path) override
  {
    return ok_result(true);
  }

  Result<bool> handle_remove_command(
    const CommandID command,
    const ChannelID channel) override
  {
    return ok_result(true);
  }
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new LinuxWorkerPlatform(thread));
}
