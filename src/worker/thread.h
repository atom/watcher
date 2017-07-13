#ifndef THREAD_H
#define THREAD_H

#include <uv.h>

#include "../queue.h"
#include "../callbacks.h"
#include "../errable.h"

class WorkerThread : public Errable {
public:
  WorkerThread(Queue &in, Queue &out, uv_async_t *main_callback);

  void run();

private:
  void work();
  void handle_events();

  Queue &in;
  Queue &out;

  uv_async_t *main_callback;

  ThreadCallback start_callback;
  uv_thread_t thread;
};

#endif
