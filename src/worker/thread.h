#ifndef THREAD_H
#define THREAD_H

#include <uv.h>

#include "../queue.h"
#include "../errable.h"

class WorkerThread : public Errable {
public:
  WorkerThread(Queue &in, Queue &out);

private:
  void start();

  Queue &in;
  Queue &out;

  uv_async_t main_callback;
  uv_thread_t thread;

  friend void start_helper(void *arg);
};

#endif
