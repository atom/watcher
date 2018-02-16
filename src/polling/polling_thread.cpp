#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <uv.h>
#include <vector>

#include "../log.h"
#include "../message_buffer.h"
#include "../result.h"
#include "../status.h"
#include "../thread.h"
#include "polled_root.h"
#include "polling_thread.h"

using std::endl;
using std::move;
using std::ostream;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

PollingThread::PollingThread(uv_async_t *main_callback) :
  Thread("polling thread", main_callback),
  poll_interval{DEFAULT_POLL_INTERVAL},
  poll_throttle{DEFAULT_POLL_THROTTLE}
{
  freeze();
}

Result<> PollingThread::init()
{
  Logger::from_env("WATCHER_LOG_POLLING");

  return ok_result();
}

Result<> PollingThread::body()
{
  while (true) {
    Timer t;

    LOGGER << "Handling commands." << endl;
    Result<size_t> cr = handle_commands();
    if (cr.is_error()) {
      LOGGER << "Unable to process incoming commands: " << cr << endl;
    } else if (is_stopping()) {
      LOGGER << "Polling thread stopping." << endl;
      return ok_result();
    }

    Result<> r = cycle();
    if (r.is_error()) {
      LOGGER << "Polling cycle failure " << r << "." << endl;
      return r.propagate_as_void();
    }

    t.stop();
    LOGGER << "Polling cycle complete in " << t << ". Sleeping for " << poll_interval.count() << "ms." << endl;
    std::this_thread::sleep_for(poll_interval);
  }
}

Result<> PollingThread::cycle()
{
  MessageBuffer buffer;
  size_t remaining = poll_throttle;

  size_t roots_left = roots.size();
  LOGGER << "Polling " << plural(roots_left, "root") << " with " << plural(poll_throttle, "throttle slot") << "."
         << endl;

  for (auto &it : roots) {
    PolledRoot &root = it.second;
    size_t allotment = remaining / roots_left;

    LOGGER << "Polling " << root << " with an allotment of " << plural(allotment, "throttle slot") << "." << endl;

    size_t progress = root.advance(buffer, allotment);
    remaining -= progress;
    if (progress != allotment) {
      LOGGER << root << " only consumed " << plural(progress, "throttle slot") << "." << endl;
    }

    roots_left--;
  }

  // Ack any commands whose roots are now fully populated.
  vector<ChannelID> to_erase;
  for (auto &split : pending_splits) {
    const ChannelID &channel_id = split.first;
    const PendingSplit &pending_split = split.second;

    size_t populated_roots = 0;
    auto channel_roots = roots.equal_range(channel_id);
    for (auto root = channel_roots.first; root != channel_roots.second; ++root) {
      if (root->second.is_all_populated()) populated_roots++;
    }

    if (populated_roots >= pending_split.second) {
      buffer.ack(pending_split.first, channel_id, true, "");
      to_erase.push_back(channel_id);
    }
  }
  for (ChannelID &channel_id : to_erase) {
    pending_splits.erase(channel_id);
  }

  return emit_all(buffer.begin(), buffer.end());
}

Result<Thread::OfflineCommandOutcome> PollingThread::handle_offline_command(const CommandPayload *command)
{
  Result<OfflineCommandOutcome> r = Thread::handle_offline_command(command);
  if (r.is_error()) return r;

  if (command->get_action() == COMMAND_ADD) {
    return ok_result(TRIGGER_RUN);
  }

  if (command->get_action() == COMMAND_POLLING_INTERVAL) {
    handle_polling_interval_command(command);
  }

  if (command->get_action() == COMMAND_POLLING_THROTTLE) {
    handle_polling_throttle_command(command);
  }

  if (command->get_action() == COMMAND_STATUS) {
    handle_status_command(command);
  }

  return ok_result(OFFLINE_ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_add_command(const CommandPayload *command)
{
  ostream &logline = LOGGER << "Adding poll root at path " << command->get_root();
  if (!command->get_recursive()) logline << " (non-recursively)";
  logline << " to channel " << command->get_channel_id() << " with " << plural(command->get_split_count(), "split")
          << "." << endl;

  roots.emplace(std::piecewise_construct,
    std::forward_as_tuple(command->get_channel_id()),
    std::forward_as_tuple(string(command->get_root()), command->get_channel_id(), command->get_recursive()));

  auto existing = pending_splits.find(command->get_channel_id());
  if (existing != pending_splits.end()) {
    bool inconsistent = false;
    string msg("Inconsistent split ADD command received by polling thread: ");

    const CommandID &existing_command_id = existing->second.first;
    const size_t &split_count = existing->second.second;

    if (existing_command_id != command->get_id()) {
      inconsistent = true;

      msg += " command ID (";
      msg += to_string(existing_command_id);
      msg += " => ";
      msg += to_string(command->get_id());
      msg += ")";
    }

    if (split_count != command->get_split_count()) {
      if (inconsistent) {
        msg += " and";
      }

      msg += " split count (";
      msg += to_string(split_count);
      msg += " => ";
      msg += to_string(command->get_split_count());
      msg += ")";
    }

    if (inconsistent) {
      return Result<CommandOutcome>::make_error(move(msg));
    }

    return ok_result(NOTHING);
  }

  if (command->get_id() != NULL_COMMAND_ID) {
    pending_splits.emplace(std::piecewise_construct,
      std::forward_as_tuple(command->get_channel_id()),
      std::forward_as_tuple(command->get_id(), command->get_split_count()));

    if (command->get_split_count() == 0u) {
      return ok_result(ACK);
    }
  }

  return ok_result(NOTHING);
}

Result<Thread::CommandOutcome> PollingThread::handle_remove_command(const CommandPayload *command)
{
  const ChannelID &channel_id = command->get_channel_id();
  LOGGER << "Removing poll roots at channel " << channel_id << "." << endl;

  roots.erase(command->get_channel_id());

  // Ensure that we ack the ADD command even if the REMOVE command arrives before all of its splits populate.
  auto pending = pending_splits.find(channel_id);
  if (pending != pending_splits.end()) {
    const PendingSplit &split = pending->second;
    const CommandID &add_command_id = split.first;

    Result<> r0 = emit(Message(AckPayload(add_command_id, channel_id, false, "Command cancelled")));
    pending_splits.erase(pending);
    if (r0.is_error()) return r0.propagate<CommandOutcome>();
  }

  if (roots.empty()) {
    LOGGER << "Final root removed." << endl;
    return ok_result(TRIGGER_STOP);
  }

  return ok_result(ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_polling_interval_command(const CommandPayload *command)
{
  poll_interval = std::chrono::milliseconds(command->get_arg());
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_polling_throttle_command(const CommandPayload *command)
{
  poll_throttle = command->get_arg();
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> PollingThread::handle_status_command(const CommandPayload *command)
{
  unique_ptr<Status> status{new Status()};

  status->polling_thread_state = state_name();
  status->polling_thread_ok = get_message();
  status->polling_in_size = get_in_queue_size();
  status->polling_in_ok = get_in_queue_error();
  status->polling_out_size = get_out_queue_size();
  status->polling_out_ok = get_out_queue_error();

  status->polling_root_count = roots.size();

  status->polling_entry_count = 0;
  for (auto &pair : roots) {
    status->polling_entry_count += pair.second.count_entries();
  }

  Result<> r = emit(Message(StatusPayload(command->get_request_id(), move(status))));
  return r.propagate(NOTHING);
}
