#ifndef POLLING_THREAD_H
#define POLLING_THREAD_H

#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <uv.h>

#include "../result.h"
#include "../status.h"
#include "../thread.h"
#include "polled_root.h"

const std::chrono::milliseconds DEFAULT_POLL_INTERVAL = std::chrono::milliseconds(100);
const uint_fast32_t DEFAULT_POLL_THROTTLE = 1000;

// The PollingThread observes filesystem changes by repeatedly calling scandir() and lstat() on registered root
// directories. It runs automatically when a `COMMAND_ADD` message is sent to it, and stops automatically when a
// `COMMAND_REMOVE` message removes the last polled root.
//
// It has a configurable "throttle" which roughly corresponds to the number of filesystem calls performed within each
// polling cycle. The throttle is distributed among polled roots so that small directories won't be starved by large
// ones.
class PollingThread : public Thread
{
public:
  explicit PollingThread(uv_async_t *main_callback);
  PollingThread(const PollingThread &) = delete;
  PollingThread(PollingThread &&) = delete;
  ~PollingThread() override = default;

  PollingThread &operator=(const PollingThread &) = delete;
  PollingThread &operator=(PollingThread &&) = delete;

private:
  Result<> body() override;

  // Perform pre-command initialization.
  Result<> init() override;

  // Perform a single polling cycle.
  Result<> cycle();

  // Wake up when a `COMMAND_ADD` message is received while stopped.
  Result<OfflineCommandOutcome> handle_offline_command(const CommandPayload *command) override;

  Result<CommandOutcome> handle_add_command(const CommandPayload *command) override;

  Result<CommandOutcome> handle_remove_command(const CommandPayload *command) override;

  // Configure the sleep interval.
  Result<CommandOutcome> handle_polling_interval_command(const CommandPayload *command) override;

  // Configure the number of system calls to perform during each `cycle()`.
  Result<CommandOutcome> handle_polling_throttle_command(const CommandPayload *command) override;

  // Respond to a request for collecting status.
  Result<CommandOutcome> handle_status_command(const CommandPayload *command) override;

  std::chrono::milliseconds poll_interval;
  uint_fast32_t poll_throttle;

  std::multimap<ChannelID, PolledRoot> roots;

  using PendingSplit = std::pair<CommandID, size_t>;
  std::map<ChannelID, PendingSplit> pending_splits;
};

#endif
