#ifndef WORKER_THREAD_H
#define WORKER_THREAD_H

#include <memory>
#include <uv.h>

#include "../queue.h"
#include "../message.h"
#include "../thread.h"

class WorkerPlatform;

class WorkerThread : public Thread {
public:
  WorkerThread(uv_async_t *main_callback);
  ~WorkerThread();

private:
  void wake() override;

  void listen();
  void handle_commands();

  uv_async_t *main_callback;

  std::unique_ptr<WorkerPlatform> platform;
  friend WorkerPlatform;
};

#endif
