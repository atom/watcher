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
    LOGGER << "Handling commands." << endl;
    Result<size_t> cr = handle_commands();
    if (cr.is_error()) {
      LOGGER << "Unable to process incoming commands: " << cr << endl;
    } else if (cr.get_value() == 0 && is_stopping()) {
      LOGGER << "Polling thread stopping." << endl;
      break;
    }

    LOGGER << "Polling root directories." << endl;
    cycle();

    if (is_healthy()) {
      LOGGER << "Sleeping for " << poll_interval.count() << "ms." << endl;
      std::this_thread::sleep_for(poll_interval);
      LOGGER << "Waking up." << endl;
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
  LOGGER << "Polling " << plural(roots_left, "root")
    << " with " << plural(poll_throttle, " throttle slot")
    << "." << endl;

  while (it != roots.end()) {
    PolledRoot &root = it->second;
    size_t allotment = remaining / roots_left;

    LOGGER << "Polling " << root
      << " with an allotment of " << plural(allotment, " throttle slot")
      << "." << endl;

    size_t progress = root.advance(buffer, allotment);
    remaining -= progress;
    LOGGER << root << " consumed "
      << progress << " " << plural(progress, " throttle slot")
      << "." << endl;

    roots_left--;
    ++it;
  }

  return emit_all(buffer.begin(), buffer.end());
}

Result<> PollingThread::handle_add_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  LOGGER << "Adding poll root at path "
    << payload->get_root()
    << " to channel " << payload->get_channel_id()
    << "." << endl;

  roots.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(payload->get_channel_id()),
    std::forward_as_tuple(string(payload->get_root()), payload->get_channel_id())
  );

  return ok_result();
}

Result<> PollingThread::handle_remove_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  LOGGER << "Removing poll root at channel "
    << payload->get_channel_id()
    << "." << endl;

  auto it = roots.find(payload->get_channel_id());
  if (it != roots.end()) roots.erase(it);

  if (roots.empty()) {
    LOGGER << "Final root removed." << endl;
    mark_stopping();
  }

  return ok_result();
}
