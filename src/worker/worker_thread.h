#ifndef WORKER_THREAD_H
#define WORKER_THREAD_H

#include <memory>
#include <uv.h>

#include "../queue.h"
#include "../message.h"
#include "../thread.h"
#include "../status.h"
#include "../result.h"

class WorkerPlatform;

class WorkerThread : public Thread {
public:
  WorkerThread(uv_async_t *main_callback);
  ~WorkerThread();

  void collect_status(Status &status) override;

private:
  Result<> wake() override;

  void listen();
  Result<> handle_commands();

  std::unique_ptr<WorkerPlatform> platform;
  friend WorkerPlatform;
};

#endif
