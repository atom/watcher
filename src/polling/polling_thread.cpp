#include <thread>
#include <chrono>
#include <string>
#include <map>
#include <utility>
#include <cstdint>
#include <uv.h>

#include "polling_thread.h"
#include "polled_root.h"
#include "../thread.h"
#include "../status.h"
#include "../result.h"
#include "../message_buffer.h"
#include "../log.h"

using std::string;
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

    cycle();

    if (is_healthy()) {
      std::this_thread::sleep_for(poll_interval);
    }
  }

  mark_stopped();
}

Result<> PollingThread::cycle()
{
  MessageBuffer buffer;
  size_t remaining = poll_throttle;

  auto it = roots.begin();
  size_t roots_left = roots.size();

  while (it != roots.end()) {
    PolledRoot &root = it->second;

    size_t allotment = remaining / roots_left;
    size_t progress = root.advance(buffer, allotment);
    remaining -= progress;

    roots_left--;
    ++it;
  }

  return emit_all(buffer.begin(), buffer.end());
}

Result<> PollingThread::handle_add_command(const CommandPayload *command, CommandOutcome &outcome)
{
  roots.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(command->get_channel_id()),
    std::forward_as_tuple(string(command->get_root()), command->get_channel_id())
  );

  return ok_result();
}

Result<> PollingThread::handle_remove_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  auto it = roots.find(payload->get_channel_id());
  if (it != roots.end()) roots.erase(it);

  if (roots.empty()) {
    mark_stopping();
  }

  return ok_result();
}
