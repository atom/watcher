#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <utility>
#include <uv.h>

#include "thread.h"
#include "message.h"
#include "result.h"
#include "log.h"

using std::string;
using std::function;
using std::unique_ptr;
using std::vector;
using std::move;
using std::endl;

void thread_callback_helper(void *arg)
{
  function<void()> *bound_fn = static_cast<std::function<void()>*>(arg);
  (*bound_fn)();
}

using CommandHandler = Result<> (Thread::*)(const CommandPayload*, Thread::CommandOutcome&);
const CommandHandler command_handlers[] = {
  [COMMAND_ADD]=&Thread::handle_add_command,
  [COMMAND_REMOVE]=&Thread::handle_remove_command,
  [COMMAND_LOG_FILE]=&Thread::handle_log_file_command,
  [COMMAND_LOG_STDERR]=&Thread::handle_log_stderr_command,
  [COMMAND_LOG_STDOUT]=&Thread::handle_log_stdout_command,
  [COMMAND_LOG_DISABLE]=&Thread::handle_log_disable_command
};

Result<> Thread::run()
{
  mark_starting();
  int err;

  err = uv_thread_create(&uv_handle, thread_callback_helper, &work_fn);
  if (err) {
    report_uv_error(err);
    return health_err_result();
  } else {
    return ok_result();
  }
}

Result<> Thread::send(Message &&message)
{
  if (!is_healthy()) return health_err_result();

  Result<> qr = in.enqueue(move(message));
  if (qr.is_error()) return qr;

  if (is_running()) {
    return wake();
  }

  if (is_stopping()) {
    uv_thread_join(&uv_handle);

    if (dead_letter_office) {
      unique_ptr<vector<Message>> dead_letters = move(dead_letter_office);
      dead_letter_office.reset(nullptr);

      return send_all(dead_letters->begin(), dead_letters->end());
    }
  }

  if (is_stopped()) {
    bool trigger = should_trigger_run(message);
    if (trigger) {
      return run();
    }
  }

  return ok_result();
}

Result< unique_ptr<vector<Message>> > Thread::receive_all()
{
  if (!is_healthy()) return health_err_result< unique_ptr<vector<Message>> >();

  return out.accept_all();
}

Result<> Thread::emit(Message &&message)
{
  if (!is_healthy()) return health_err_result();

  Result<> qr = out.enqueue(move(message));
  if (qr.is_error()) return qr;

  int uv_err = uv_async_send(main_callback);
  if (uv_err) {
    return error_result(uv_strerror(uv_err));
  }

  return ok_result();
}

Result<size_t> Thread::handle_commands()
{
  Result< unique_ptr<vector<Message>> > pr = in.accept_all();
  if (pr.is_error()) {
    return pr.propagate<size_t>();
  }
  unique_ptr<vector<Message>> &accepted = pr.get_value();
  if (!accepted) {
    // No command messages to accept.
    return ok_result(static_cast<size_t>(0));
  }

  vector<Message> acks;
  acks.reserve(accepted->size());
  bool should_stop = false;

  for (Message &message : *accepted) {
    const CommandPayload *command = message.as_command();
    if (!command) {
      LOGGER << "Received unexpected non-command message " << message << "." << endl;
      continue;
    }

    CommandOutcome outcome = {
      true, // ack
      true, // success
      should_stop // should_stop
    };
    string m = "";

    CommandHandler handler = command_handlers[command->get_action()];
    if (handler == nullptr) {
      handler = &Thread::handle_unknown_command;
    }
    Result<> hr = (this->*handler)(command, outcome);
    if (hr.is_error()) {
      LOGGER << "Reporting command handler error: " << hr << "." << endl;
      outcome.success = false;
      m = move(hr.get_error());
    }

    if (outcome.ack) {
      AckPayload ack(command->get_id(), command->get_channel_id(), outcome.success, move(m));
      Message response(move(ack));
      acks.push_back(move(response));
    }

    should_stop = outcome.should_stop;
  }

  Result<> er = emit_all(acks.begin(), acks.end());
  if (er.is_error()) return er.propagate<size_t>(accepted->size());

  if (should_stop) {
    mark_stopping();

    // Move any messages enqueued since we picked up this batch of commands into the dead letter office.
    Result< unique_ptr<vector<Message>> > dr = in.accept_all();
    if (dr.is_error()) dr.propagate<size_t>();

    dead_letter_office = move(dr.get_value());
  }

  return ok_result(static_cast<size_t>(accepted->size()));
}

Result<> Thread::handle_add_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  return handle_unknown_command(payload, outcome);
}

Result<> Thread::handle_remove_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  return handle_unknown_command(payload, outcome);
}

Result<> Thread::handle_log_file_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  Logger::to_file(payload->get_root().c_str());
  return ok_result();
}

Result<> Thread::handle_log_stderr_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  Logger::to_stderr();
  return ok_result();
}

Result<> Thread::handle_log_stdout_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  Logger::to_stdout();
  return ok_result();
}

Result<> Thread::handle_log_disable_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  Logger::to_stderr();
  return ok_result();
}

Result<> Thread::handle_unknown_command(const CommandPayload *payload, CommandOutcome &outcome)
{
  LOGGER << "Received command with unexpected action " << *payload << "." << endl;
  return ok_result();
}
