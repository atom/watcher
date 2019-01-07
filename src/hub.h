#ifndef HUB_H
#define HUB_H

#include <memory>
#include <nan.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <uv.h>

#include "errable.h"
#include "log.h"
#include "message.h"
#include "nan/async_callback.h"
#include "polling/polling_thread.h"
#include "result.h"
#include "worker/worker_thread.h"

class Hub : public Errable
{
public:
  static Hub *get()
  {
    if (the_hub == nullptr) {
      the_hub = new Hub();
    }
    return the_hub;
  }

  Hub(const Hub &) = delete;
  Hub(Hub &&) = delete;
  ~Hub() override = default;

  Hub &operator=(const Hub &) = delete;
  Hub &operator=(Hub &&) = delete;

  Result<> use_main_log_file(std::string &&main_log_file)
  {
    Result<> h = health_err_result();
    if (h.is_error()) return h;

    std::string r = Logger::to_file(main_log_file.c_str());
    return r.empty() ? ok_result() : error_result(std::move(r));
  }

  Result<> use_main_log_stderr()
  {
    Result<> h = health_err_result();
    if (h.is_error()) return h;

    std::string r = Logger::to_stderr();
    return r.empty() ? ok_result() : error_result(std::move(r));
  }

  Result<> use_main_log_stdout()
  {
    Result<> h = health_err_result();
    if (h.is_error()) return h;

    std::string r = Logger::to_stdout();
    return r.empty() ? ok_result() : error_result(std::move(r));
  }

  Result<> disable_main_log()
  {
    Result<> h = health_err_result();
    if (h.is_error()) return h;

    std::string r = Logger::disable();
    return r.empty() ? ok_result() : error_result(std::move(r));
  }

  Result<> use_worker_log_file(std::string &&worker_log_file, std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(
      worker_thread, CommandPayloadBuilder::log_to_file(std::move(worker_log_file)), std::move(callback));
  }

  Result<> use_worker_log_stderr(std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(worker_thread, CommandPayloadBuilder::log_to_stderr(), std::move(callback));
  }

  Result<> use_worker_log_stdout(std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(worker_thread, CommandPayloadBuilder::log_to_stdout(), std::move(callback));
  }

  Result<> disable_worker_log(std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(worker_thread, CommandPayloadBuilder::log_disable(), std::move(callback));
  }

  Result<> worker_cache_size(size_t cache_size, std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(worker_thread, CommandPayloadBuilder::cache_size(cache_size), std::move(callback));
  }

  Result<> use_polling_log_file(std::string &&polling_log_file, std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(
      polling_thread, CommandPayloadBuilder::log_to_file(std::move(polling_log_file)), std::move(callback));
  }

  Result<> use_polling_log_stderr(std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(polling_thread, CommandPayloadBuilder::log_to_stderr(), std::move(callback));
  }

  Result<> use_polling_log_stdout(std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(polling_thread, CommandPayloadBuilder::log_to_stdout(), std::move(callback));
  }

  Result<> disable_polling_log(std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(polling_thread, CommandPayloadBuilder::log_disable(), std::move(callback));
  }

  Result<> set_polling_interval(uint_fast32_t interval, std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(polling_thread, CommandPayloadBuilder::polling_interval(interval), std::move(callback));
  }

  Result<> set_polling_throttle(uint_fast32_t throttle, std::unique_ptr<AsyncCallback> callback)
  {
    if (!check_async(callback)) return ok_result();

    return send_command(polling_thread, CommandPayloadBuilder::polling_throttle(throttle), std::move(callback));
  }

  Result<> watch(std::string &&root,
    bool poll,
    bool recursive,
    std::unique_ptr<AsyncCallback> ack_callback,
    std::unique_ptr<AsyncCallback> event_callback);

  Result<> unwatch(ChannelID channel_id, std::unique_ptr<AsyncCallback> &&ack_callback);

  Result<> status(std::unique_ptr<AsyncCallback> &&status_callback);

  void handle_events();

private:
  struct StatusReq
  {
    explicit StatusReq(std::unique_ptr<AsyncCallback> &&callback) : callback{std::move(callback)}
    {
      //
    }

    ~StatusReq() = default;

    StatusReq(const StatusReq &) = delete;
    StatusReq(StatusReq &&) = delete;

    StatusReq &operator=(const StatusReq &) = delete;
    StatusReq &operator=(StatusReq &&) = delete;

    Status status;
    std::unique_ptr<AsyncCallback> callback;
  };

  Hub();

  Result<> send_command(Thread &thread, CommandPayloadBuilder &&builder, std::unique_ptr<AsyncCallback> callback);

  bool check_async(const std::unique_ptr<AsyncCallback> &callback);

  void handle_events_from(Thread &thread);

  void handle_completed_status(StatusReq &req);

  static Hub *the_hub;

  uv_async_t event_handler{};

  WorkerThread worker_thread;
  PollingThread polling_thread;

  CommandID next_command_id;
  ChannelID next_channel_id;
  RequestID next_request_id;

  std::unordered_map<CommandID, std::unique_ptr<AsyncCallback>> pending_callbacks;
  std::unordered_map<RequestID, std::unique_ptr<StatusReq>> status_reqs;
  std::unordered_map<ChannelID, std::shared_ptr<AsyncCallback>> channel_callbacks;
};

#endif
