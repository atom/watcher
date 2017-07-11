#ifndef THREAD_H
#define THREAD_H

#include <uv.h>

#include "../errable.h"

class WorkerThread : public Errable {
public:
  WorkerThread();

private:
  void start();

  uv_thread_t thread;

  friend void helper(void *arg);
};

#endif
