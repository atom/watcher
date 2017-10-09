#ifndef WORKER_THREAD_H
#define WORKER_THREAD_H

#include <memory>
#include <uv.h>

#include "../message.h"
#include "../queue.h"
#include "../result.h"
#include "../status.h"
#include "../thread.h"

class WorkerPlatform;

class WorkerThread : public Thread
{
public:
  WorkerThread(uv_async_t *main_callback);

  ~WorkerThread();

  void collect_status(Status &status) override;

private:
  Result<> wake() override;

  Result<> body() override;

  Result<CommandOutcome> handle_add_command(const CommandPayload *command) override;

  Result<CommandOutcome> handle_remove_command(const CommandPayload *payload) override;

  std::unique_ptr<WorkerPlatform> platform;

  friend WorkerPlatform;
};

#endif
