#include <memory>
#include <string>
#include <uv.h>
#include <vector>

#include "../log.h"
#include "../message.h"
#include "../queue.h"
#include "../result.h"
#include "worker_platform.h"
#include "worker_thread.h"

using std::string;
using std::unique_ptr;

WorkerThread::WorkerThread(uv_async_t *main_callback) :
  Thread("worker thread", main_callback),
  platform{WorkerPlatform::for_worker(this)}
{
  //
}

// Definition must be here to see the full definition of WorkerPlatform.
WorkerThread::~WorkerThread() = default;

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
  status.worker_thread_state = state_name();
  status.worker_thread_ok = get_error();
  status.worker_in_size = get_in_queue_size();
  status.worker_in_ok = get_in_queue_error();
  status.worker_out_size = get_out_queue_size();
  status.worker_out_ok = get_out_queue_error();
}
