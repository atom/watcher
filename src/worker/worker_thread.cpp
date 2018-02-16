#include <memory>
#include <string>
#include <uv.h>
#include <vector>

#include "../log.h"
#include "../message.h"
#include "../queue.h"
#include "../result.h"
#include "../status.h"
#include "worker_platform.h"
#include "worker_thread.h"

using std::string;
using std::unique_ptr;

WorkerThread::WorkerThread(uv_async_t *main_callback) :
  Thread("worker thread", main_callback),
  platform{WorkerPlatform::for_worker(this)}
{
  report_errable(*platform);
  freeze();
}

// Definition must be here to see the full definition of WorkerPlatform.
WorkerThread::~WorkerThread() = default;

Result<> WorkerThread::wake()
{
  return platform->wake();
}

Result<> WorkerThread::init()
{
  Logger::from_env("WATCHER_LOG_WORKER");

  return platform->init();
}

Result<> WorkerThread::body()
{
  return platform->listen();
}

Result<Thread::CommandOutcome> WorkerThread::handle_add_command(const CommandPayload *payload)
{
  Result<bool> r = platform->handle_add_command(
    payload->get_id(), payload->get_channel_id(), payload->get_root(), payload->get_recursive());
  return r.is_ok() ? r.propagate(r.get_value() ? ACK : NOTHING) : r.propagate<CommandOutcome>();
}

Result<Thread::CommandOutcome> WorkerThread::handle_remove_command(const CommandPayload *payload)
{
  Result<bool> r = platform->handle_remove_command(payload->get_id(), payload->get_channel_id());
  return r.propagate(r.get_value() ? ACK : NOTHING);
}

Result<Thread::CommandOutcome> WorkerThread::handle_cache_size_command(const CommandPayload *payload)
{
  platform->handle_cache_size_command(payload->get_arg());
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> WorkerThread::handle_status_command(const CommandPayload *payload)
{
  unique_ptr<Status> status{new Status()};

  status->worker_thread_state = state_name();
  status->worker_thread_ok = get_message();
  status->worker_in_size = get_in_queue_size();
  status->worker_in_ok = get_in_queue_error();
  status->worker_out_size = get_out_queue_size();
  status->worker_out_ok = get_out_queue_error();

  platform->populate_status(*status);

  Result<> r = emit(Message(StatusPayload(payload->get_request_id(), move(status))));
  return r.propagate(NOTHING);
}
