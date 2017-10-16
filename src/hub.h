#ifndef HUB_H
#define HUB_H

#include <memory>
#include <nan.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <uv.h>

#include "log.h"
#include "message.h"
#include "polling/polling_thread.h"
#include "result.h"
#include "worker/worker_thread.h"

class Hub
{
public:
  static Hub &get() { return the_hub; }

  Hub(const Hub &) = delete;
  Hub(Hub &&) = delete;
  ~Hub() = default;

  Hub &operator=(const Hub &) = delete;
  Hub &operator=(Hub &&) = delete;

  void use_main_log_file(std::string &&main_log_file) { Logger::to_file(main_log_file.c_str()); }

  void use_main_log_stderr() { Logger::to_stderr(); }

  void use_main_log_stdout() { Logger::to_stdout(); }

  void disable_main_log() { Logger::disable(); }

  Result<> use_worker_log_file(std::string &&worker_log_file, std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(
      worker_thread, CommandPayloadBuilder::log_to_file(std::move(worker_log_file)), std::move(callback));
  }

  Result<> use_worker_log_stderr(std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(worker_thread, CommandPayloadBuilder::log_to_stderr(), std::move(callback));
  }

  Result<> use_worker_log_stdout(std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(worker_thread, CommandPayloadBuilder::log_to_stdout(), std::move(callback));
  }

  Result<> disable_worker_log(std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(worker_thread, CommandPayloadBuilder::log_to_stderr(), std::move(callback));
  }

  Result<> use_polling_log_file(std::string &&polling_log_file, std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(
      polling_thread, CommandPayloadBuilder::log_to_file(std::move(polling_log_file)), std::move(callback));
  }

  Result<> use_polling_log_stderr(std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(polling_thread, CommandPayloadBuilder::log_to_stderr(), std::move(callback));
  }

  Result<> use_polling_log_stdout(std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(polling_thread, CommandPayloadBuilder::log_to_stdout(), std::move(callback));
  }

  Result<> disable_polling_log(std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(polling_thread, CommandPayloadBuilder::log_disable(), std::move(callback));
  }

  Result<> set_polling_interval(uint_fast32_t interval, std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(polling_thread, CommandPayloadBuilder::polling_interval(interval), std::move(callback));
  }

  Result<> set_polling_throttle(uint_fast32_t throttle, std::unique_ptr<Nan::Callback> callback)
  {
    return send_command(polling_thread, CommandPayloadBuilder::polling_throttle(throttle), std::move(callback));
  }

  Result<> watch(std::string &&root,
    bool poll,
    bool recursive,
    std::unique_ptr<Nan::Callback> ack_callback,
    std::unique_ptr<Nan::Callback> event_callback);

  Result<> unwatch(ChannelID channel_id, std::unique_ptr<Nan::Callback> &&ack_callback);

  void handle_events();

  void collect_status(Status &status);

private:
  Hub();

  Result<> send_command(Thread &thread, CommandPayloadBuilder &&builder, std::unique_ptr<Nan::Callback> callback);

  void handle_events_from(Thread &thread);

  static Hub the_hub;

  uv_async_t event_handler{};

  WorkerThread worker_thread;
  PollingThread polling_thread;

  CommandID next_command_id;
  ChannelID next_channel_id;

  std::unordered_map<CommandID, std::unique_ptr<Nan::Callback>> pending_callbacks;
  std::unordered_map<ChannelID, std::shared_ptr<Nan::Callback>> channel_callbacks;
};

#endif
