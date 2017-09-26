#ifndef THREAD_H
#define THREAD_H

#include <atomic>
#include <string>
#include <memory>
#include <utility>
#include <functional>
#include <vector>
#include <uv.h>

#include "errable.h"
#include "result.h"
#include "queue.h"
#include "message.h"
#include "status.h"

void thread_callback_helper(void *arg);

class Thread : public SyncErrable {
public:
  template< class T >
  Thread(T* self, void (T::*fn)(), std::string name, uv_async_t *main_callback) :
    SyncErrable(name),
    in(name + " input queue"),
    out(name + " output queue"),
    main_callback{main_callback},
    work_fn{std::bind(std::mem_fn(fn), self)},
    state{State::STOPPED}
  {
    //
  };

  Result<> run();

  Result<> send(Message &&message);

  template <class InputIt>
  Result<> send_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return health_err_result();

    Result<> qr = in.enqueue_all(begin, end);
    if (qr.is_error()) return qr;

    if (is_running() || is_stopping()) {
      Result<> wr = wake();
      if (wr.is_error()) return wr;
    }

    if (is_stopped()) {
      bool trigger = false;
      for (auto it = begin; it != end; ++it) {
        if (should_trigger_run(*it)) {
          trigger = true;
          break;
        }
      }

      if (trigger) {
        Result<> rr = run();
        if (rr.is_error()) return rr;
      }
    }

    return ok_result();
  }

  Result< std::unique_ptr<std::vector<Message>> > receive_all();

  virtual void collect_status(Status &status) = 0;

  struct CommandOutcome {
    bool ack;
    bool success;
  };

  virtual Result<> handle_add_command(const CommandPayload *payload, CommandOutcome &outcome);
  virtual Result<> handle_remove_command(const CommandPayload *payload, CommandOutcome &outcome);
  Result<> handle_log_file_command(const CommandPayload *payload, CommandOutcome &outcome);
  Result<> handle_log_stderr_command(const CommandPayload *payload, CommandOutcome &outcome);
  Result<> handle_log_stdout_command(const CommandPayload *payload, CommandOutcome &outcome);
  Result<> handle_log_disable_command(const CommandPayload *payload, CommandOutcome &outcome);
  Result<> handle_unknown_command(const CommandPayload *payload, CommandOutcome &outcome);

protected:
  virtual Result<> wake()
  {
    return ok_result();
  }

  Result<> emit(Message &&message);

  template <class InputIt>
  Result<> emit_all(InputIt begin, InputIt end)
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

  Result<size_t> handle_commands();

  void mark_stopped() { state.store(State::STOPPED); }
  void mark_starting() { state.store(State::STARTING); }
  void mark_running() { state.store(State::RUNNING); }
  void mark_stopping() { state.store(State::STOPPING); }

  bool is_starting() { return state.load() == State::STARTING; }
  bool is_running() { return state.load() == State::RUNNING; }
  bool is_stopped() { return state.load() == State::STOPPED; }
  bool is_stopping() { return state.load() == State::STOPPING; }

  virtual bool should_trigger_run(Message &message) { return false; }

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

  Queue in;
  Queue out;

  uv_async_t *main_callback;

  uv_thread_t uv_handle;
  std::function<void()> work_fn;

  std::atomic<State> state;
};

#endif
