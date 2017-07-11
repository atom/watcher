#include <iostream>
#include <uv.h>

#include "thread.h"
#include "../log.h"

using std::endl;

void helper(void *arg)
{
  WorkerThread *worker = (WorkerThread*) arg;
  worker->start();
}

WorkerThread::WorkerThread()
{
  int err;

  err = uv_thread_create(&thread, helper, (void*) this);
  report_uv_error(err);
}

void WorkerThread::start()
{
  Logger::toFile("worker.log");

  LOGGER << "What's up" << endl;
}
