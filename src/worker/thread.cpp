#include <iostream>
#include <uv.h>

#include "thread.h"
#include "../log.h"
#include "../queue.h"

using std::endl;

void start_helper(void *arg)
{
  WorkerThread *worker = (WorkerThread*) arg;
  worker->start();
}

WorkerThread::WorkerThread(Queue &in, Queue &out) :
  in{in}, out{out}
{
  int err;

  err = uv_thread_create(&thread, start_helper, (void*) this);
  report_uv_error(err);
}

void WorkerThread::start()
{
  Logger::toFile("worker.log");

  LOGGER << "What's up" << endl;
}
