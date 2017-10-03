#include <vector>
#include <memory>
#include <iostream>
#include <utility>
#include <string>
#include <uv.h>

#include "worker_thread.h"
#include "worker_platform.h"
#include "../log.h"
#include "../queue.h"
#include "../message.h"
#include "../result.h"

using std::endl;
using std::vector;
using std::unique_ptr;
using std::move;
using std::string;

WorkerThread::WorkerThread(uv_async_t *main_callback) :
  Thread("worker thread", main_callback),
  platform{WorkerPlatform::for_worker(this)}
{
  //
}

WorkerThread::~WorkerThread()
{
  // Necessary so that unique_ptr can see the full definition of WorkerPlatform
}

Result<> WorkerThread::wake()
{
  if (!is_healthy()) return health_err_result();

  return platform->wake();
}

Result<> WorkerThread::body()
{
  return platform->listen();
}

Result<Thread::CommandOutcome> WorkerThread::handle_add_command(const CommandPayload *payload)
{
  Result<bool> r = platform->handle_add_command(payload->get_id(), payload->get_channel_id(), payload->get_root());
  if (r.is_error()) return r.propagate<CommandOutcome>();

  return ok_result(r.get_value() ? ACK : NOTHING);
}

Result<Thread::CommandOutcome> WorkerThread::handle_remove_command(const CommandPayload *payload)
{
  Result<bool> r = platform->handle_remove_command(payload->get_id(), payload->get_channel_id());
  if (r.is_error()) return r.propagate<CommandOutcome>();

  return ok_result(r.get_value() ? ACK : NOTHING);
}

void WorkerThread::collect_status(Status &status)
{
  status.worker_thread_ok = get_error();
  status.worker_in_size = get_in_queue_size();
  status.worker_in_ok = get_in_queue_error();
  status.worker_out_size = get_out_queue_size();
  status.worker_out_ok = get_out_queue_error();
}
