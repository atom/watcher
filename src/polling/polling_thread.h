#ifndef POLLING_THREAD_H
#define POLLING_THREAD_H

#include <cstdint>
#include <chrono>
#include <map>
#include <uv.h>

#include "../thread.h"
#include "../status.h"
#include "../result.h"
#include "polled_root.h"

const std::chrono::milliseconds DEFAULT_POLL_INTERVAL = std::chrono::milliseconds(500);
const uint_fast64_t DEFAULT_POLL_THROTTLE = 1000;

class PollingThread : public Thread {
public:
  PollingThread(uv_async_t *main_callback);
  ~PollingThread();

  void collect_status(Status &status) override;

private:
  Result<> body() override;

  Result<> cycle();

  Result<OfflineCommandOutcome> handle_offline_command(const CommandPayload *command) override;

  Result<CommandOutcome> handle_add_command(const CommandPayload *command) override;

  Result<CommandOutcome> handle_remove_command(const CommandPayload *payload) override;

  std::chrono::milliseconds poll_interval;
  uint_fast64_t poll_throttle;

  std::map<ChannelID, PolledRoot> roots;
};

#endif
