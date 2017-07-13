#ifndef THREAD_H
#define THREAD_H

#include <uv.h>

#include "../queue.h"
#include "../callbacks.h"
#include "../errable.h"

class WorkerPlatform;

class WorkerThread : public Errable {
public:
  WorkerThread(Queue &in, Queue &out, uv_async_t *main_callback);
  ~WorkerThread();

  void run();
  void wake();

  void handle_commands();

private:
  void listen();

  Queue &in;
  Queue &out;

  uv_async_t *main_callback;

  ThreadCallback start_callback;
  uv_thread_t thread;

  WorkerPlatform *platform;
};

#endif
