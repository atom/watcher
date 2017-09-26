#include <thread>
#include <chrono>
#include <cstdint>
#include <uv.h>

#include "polling_thread.h"
#include "../thread.h"
#include "../status.h"
#include "../result.h"
#include "../log.h"

using std::endl;

PollingThread::PollingThread(uv_async_t *main_callback) :
  Thread(this, &PollingThread::poll, "polling thread", main_callback),
  poll_interval{DEFAULT_POLL_INTERVAL},
  poll_throttle{DEFAULT_POLL_THROTTLE}
{
  //
}

PollingThread::~PollingThread()
{
  //
}

void PollingThread::collect_status(Status &status)
{
  status.polling_thread_ok = get_error();
  status.polling_in_size = get_in_queue_size();
  status.polling_in_ok = get_in_queue_error();
  status.polling_out_size = get_out_queue_size();
  status.polling_out_ok = get_out_queue_error();
}

bool PollingThread::should_trigger_run(Message &message)
{
  const CommandPayload *payload = message.as_command();
  if (!payload) return false;

  return payload->get_action() == COMMAND_ADD;
}

void PollingThread::poll()
{
  mark_running();

  while (true) {
    Result<size_t> cr = handle_commands();
    if (cr.is_error()) {
      LOGGER << "Unable to process incoming commands: " << cr << endl;
    } else if (cr.get_value() == 0 && is_stopping()) {
      LOGGER << "Polling thread stopping." << endl;
      break;
    }

    // TODO: Sample filesystem

    if (is_healthy()) {
      std::this_thread::sleep_for(poll_interval);
    }
  }

  mark_stopped();
}

Result<> PollingThread::handle_add_command(const CommandPayload *command, CommandOutcome &outcome)
{
  return ok_result();
}

Result<> PollingThread::handle_remove_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  return ok_result();
}
