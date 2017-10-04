#ifndef THREAD_H
#define THREAD_H

#include <atomic>
#include <string>
#include <memory>
#include <utility>
#include <functional>
#include <vector>
#include <iostream>
#include <sstream>
#include <uv.h>

#include "errable.h"
#include "result.h"
#include "queue.h"
#include "message.h"
#include "status.h"
#include "thread_starter.h"

class Thread : public SyncErrable {
public:
  Thread(
    std::string name,
    uv_async_t *main_callback,
    std::unique_ptr<ThreadStarter> starter = std::unique_ptr<ThreadStarter>(new ThreadStarter())
  );

  Result<> run();

  Result<bool> send(Message &&message);

  template <class InputIt>
  Result<bool> send_all(InputIt begin, InputIt end);

  Result< std::unique_ptr<std::vector<Message>> > receive_all();

  Result<bool> drain();

  virtual void collect_status(Status &status) = 0;

protected:
  void start();

  virtual Result<> body() { return ok_result(); }

  virtual Result<> wake() { return ok_result(); }

  Result<> emit(Message &&message);

  template <class InputIt>
  Result<> emit_all(InputIt begin, InputIt end);

  enum CommandOutcome {
    NOTHING,
    ACK,
    TRIGGER_STOP,
    PREVENT_STOP
  };

  enum OfflineCommandOutcome {
    OFFLINE_ACK,
    TRIGGER_RUN
  };

  Result<size_t> handle_commands();

  virtual Result<CommandOutcome> handle_add_command(const CommandPayload *payload);

  virtual Result<CommandOutcome> handle_remove_command(const CommandPayload *payload);

  Result<CommandOutcome> handle_log_file_command(const CommandPayload *payload);

  Result<CommandOutcome> handle_log_stderr_command(const CommandPayload *payload);

  Result<CommandOutcome> handle_log_stdout_command(const CommandPayload *payload);

  Result<CommandOutcome> handle_log_disable_command(const CommandPayload *payload);

  Result<CommandOutcome> handle_unknown_command(const CommandPayload *payload);

  virtual Result<OfflineCommandOutcome> handle_offline_command(const CommandPayload *payload);

  using CommandHandler = Result<CommandOutcome> (Thread::*)(const CommandPayload*);
  static const CommandHandler command_handlers[];

  void mark_stopped() { state.store(State::STOPPED); }
  void mark_starting() { state.store(State::STARTING); }
  void mark_running() { state.store(State::RUNNING); }
  void mark_stopping() { state.store(State::STOPPING); }

  bool is_starting() { return state.load() == State::STARTING; }
  bool is_running() { return state.load() == State::RUNNING; }
  bool is_stopped() { return state.load() == State::STOPPED; }
  bool is_stopping() { return state.load() == State::STOPPING; }

  std::string state_name();

  std::string get_in_queue_error() { return in.get_error(); }
  size_t get_in_queue_size() { return in.size(); }
  std::string get_out_queue_error() { return out.get_error(); }
  size_t get_out_queue_size() { return out.size(); }

private:
  enum State {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING
  };

  std::atomic<State> state;

  std::unique_ptr<ThreadStarter> starter;

  Queue in;
  Queue out;

  uv_async_t *main_callback;

  uv_thread_t uv_handle;
  std::function<void()> work_fn;

  std::unique_ptr<std::vector<Message>> dead_letter_office;
};

template <class InputIt>
Result<bool> Thread::send_all(InputIt begin, InputIt end)
{
  if (!is_healthy()) return health_err_result<bool>();

  if (is_stopping()) {
    uv_thread_join(&uv_handle);

    if (dead_letter_office) {
      std::unique_ptr<std::vector<Message>> dead_letters = std::move(dead_letter_office);
      dead_letter_office.reset(nullptr);

      for (InputIt it = begin; it != end; ++it) {
        dead_letters->emplace_back(std::move(*it));
      }

      return send_all(dead_letters->begin(), dead_letters->end());
    }
  }

  if (is_stopped()) {
    bool should_run = false;
    std::vector<Message> acks;

    for (InputIt it = begin; it != end; ++it) {
      const CommandPayload *command = it->as_command();
      if (command == nullptr) {
        std::ostringstream m;
        m << "Non-command message " << *it << " sent";
        acks.emplace_back(Message::ack(*it, false, m.str()));
        continue;
      }

      Result<OfflineCommandOutcome> r0 = handle_offline_command(command);
      if (r0.is_error() || r0.get_value() == OFFLINE_ACK) {
        acks.emplace_back(Message::ack(*it, r0.propagate_as_void()));
      } else if (r0.get_value() == TRIGGER_RUN) {
        Result<> r1 = in.enqueue(std::move(*it));
        if (r1.is_error()) return r1.propagate<bool>();

        should_run = true;
      } else {
        std::ostringstream m;
        m << "Message " << *it << " returned invalid offline outcome " << r0;
        acks.emplace_back(Message::ack(*it, false, m.str()));
      }
    }

    if (!acks.empty()) {
      Result<> r2 = out.enqueue_all(acks.begin(), acks.end());
      if (r2.is_error()) return r2.propagate<bool>();
    }

    if (should_run) {
      return run().propagate(!acks.empty());
    }

    return ok_result(!acks.empty());
  }

  Result<> r3 = in.enqueue_all(begin, end);
  if (r3.is_error()) return r3.template propagate<bool>();

  if (is_running()) {
    return wake().propagate(false);
  }

  return ok_result(false);
}

template <class InputIt>
Result<> Thread::emit_all(InputIt begin, InputIt end)
{
  if (!is_healthy()) return health_err_result();

  Result<> qr = out.enqueue_all(begin, end);
  if (qr.is_error()) return qr;

  int uv_err = uv_async_send(main_callback);
  if (uv_err) {
    return error_result(uv_strerror(uv_err));
  }

  return ok_result();
}

std::ostream &operator<<(std::ostream &out, const Thread &th);

#endif
