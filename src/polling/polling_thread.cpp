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
  Thread("polling thread", main_callback),
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
  status.polling_thread_state = state_name();
  status.polling_thread_ok = get_error();
  status.polling_in_size = get_in_queue_size();
  status.polling_in_ok = get_in_queue_error();
  status.polling_out_size = get_out_queue_size();
  status.polling_out_ok = get_out_queue_error();
}

Result<> PollingThread::body()
{
  while (true) {
    LOGGER << "Handling commands." << endl;
    Result<size_t> cr = handle_commands();
    if (cr.is_error()) {
      LOGGER << "Unable to process incoming commands: " << cr << endl;
    } else if (is_stopping()) {
      LOGGER << "Polling thread stopping." << endl;
      return ok_result();
    }

    LOGGER << "Polling root directories." << endl;
    cycle();

    if (is_healthy()) {
      LOGGER << "Sleeping for " << poll_interval.count() << "ms." << endl;
      std::this_thread::sleep_for(poll_interval);
      LOGGER << "Waking up." << endl;
    }
  }
}

Result<> PollingThread::cycle()
{
  MessageBuffer buffer;
  size_t remaining = poll_throttle;

  auto it = roots.begin();
  size_t roots_left = roots.size();
  LOGGER << "Polling " << plural(roots_left, "root")
    << " with " << plural(poll_throttle, "throttle slot")
    << "." << endl;

  while (it != roots.end()) {
    PolledRoot &root = it->second;
    size_t allotment = remaining / roots_left;

    LOGGER << "Polling " << root
      << " with an allotment of " << plural(allotment, "throttle slot") << "." << endl;

    size_t progress = root.advance(buffer, allotment);
    remaining -= progress;
    LOGGER << root << " consumed " << plural(progress, "throttle slot") << "." << endl;

    roots_left--;
    ++it;
  }

  return emit_all(buffer.begin(), buffer.end());
}

Result<Thread::OfflineCommandOutcome> PollingThread::handle_offline_command(const CommandPayload *payload)
{
  Result<OfflineCommandOutcome> r = Thread::handle_offline_command(payload);
  if (r.is_error()) return r;

  if (payload->get_action() == COMMAND_ADD) {
    return ok_result(TRIGGER_RUN);
  }

  return ok_result(OFFLINE_ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_add_command(const CommandPayload *payload)
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

  return ok_result(ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_remove_command(const CommandPayload *payload)
{
  LOGGER << "Removing poll root at channel "
    << payload->get_channel_id()
    << "." << endl;

  auto it = roots.find(payload->get_channel_id());
  if (it != roots.end()) roots.erase(it);

  if (roots.empty()) {
    LOGGER << "Final root removed." << endl;
    return ok_result(TRIGGER_STOP);
  }

  return ok_result(ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_polling_interval_command(const CommandPayload *payload)
{
  poll_interval = std::chrono::milliseconds(payload->get_arg());
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_polling_throttle_command(const CommandPayload *payload)
{
  poll_throttle = payload->get_arg();
  return ok_result(ACK);
}
