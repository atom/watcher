#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <uv.h>
#include <vector>

#include "log.h"
#include "message.h"
#include "result.h"
#include "thread.h"

using std::bind;
using std::endl;
using std::function;
using std::move;
using std::ostream;
using std::ostringstream;
using std::string;
using std::unique_ptr;
using std::vector;

void thread_callback_helper(void *arg)
{
  auto *bound_fn = static_cast<std::function<void()> *>(arg);
  (*bound_fn)();
}

Thread::DispatchTable::DispatchTable()
{
  handlers[COMMAND_ADD] = &Thread::handle_add_command;
  handlers[COMMAND_REMOVE] = &Thread::handle_remove_command;
  handlers[COMMAND_LOG_FILE] = &Thread::handle_log_file_command;
  handlers[COMMAND_LOG_STDERR] = &Thread::handle_log_stderr_command;
  handlers[COMMAND_LOG_STDOUT] = &Thread::handle_log_stdout_command;
  handlers[COMMAND_LOG_DISABLE] = &Thread::handle_log_disable_command;
  handlers[COMMAND_POLLING_INTERVAL] = &Thread::handle_polling_interval_command;
  handlers[COMMAND_POLLING_THROTTLE] = &Thread::handle_polling_throttle_command;
  handlers[COMMAND_CACHE_SIZE] = &Thread::handle_cache_size_command;
  handlers[COMMAND_DRAIN] = &Thread::handle_unknown_command;
  handlers[COMMAND_STATUS] = &Thread::handle_status_command;
}

const Thread::DispatchTable Thread::command_handlers;

Thread::Thread(std::string &&name, uv_async_t *main_callback, unique_ptr<ThreadStarter> starter) :
  name{move(name)},
  state{State::STOPPED},
  starter{move(starter)},
  main_callback{main_callback},
  work_fn{bind(&Thread::start, this)}
{
  report_errable(in);
  report_errable(out);
};

Result<> Thread::run()
{
  mark_starting();
  int err;

  err = uv_thread_create(&uv_handle, thread_callback_helper, &work_fn);
  if (err != 0) {
    return error_result(uv_strerror(err));
  }

  return ok_result();
}

Result<bool> Thread::send(Message &&message)
{
  if (is_stopping()) {
    uv_thread_join(&uv_handle);

    if (dead_letter_office) {
      unique_ptr<vector<Message>> dead_letters = move(dead_letter_office);
      dead_letter_office.reset(nullptr);
      dead_letters->emplace_back(move(message));

      return send_all(dead_letters->begin(), dead_letters->end());
    }
  }

  if (is_stopped()) {
    const CommandPayload *command = message.as_command();
    if (command == nullptr) {
      ostringstream m;
      m << "Non-command message " << message << " sent to a stopped thread";

      out.enqueue(Message::ack(message, false, m.str()));
      return ok_result(true);
    }

    LOGGER << "Processing offline command: " << *command << "." << endl;
    Result<OfflineCommandOutcome> r0 = handle_offline_command(command);
    LOGGER << "Result: " << r0 << "." << endl;
    if (r0.is_error() || r0.get_value() == OFFLINE_ACK) {
      out.enqueue(Message::ack(message, r0.propagate_as_void()));
      return ok_result(true);
    }

    if (r0.get_value() == TRIGGER_RUN) {
      in.enqueue(move(message));
      return run().propagate(false);
    }
  }

  in.enqueue(move(message));

  if (is_running()) {
    return wake().propagate(false);
  }

  return ok_result(false);
}

unique_ptr<vector<Message>> Thread::receive_all()
{
  return out.accept_all();
}

Result<bool> Thread::drain()
{
  if (is_stopping()) {
    uv_thread_join(&uv_handle);
  }

  if (is_stopped()) {
    if (dead_letter_office) {
      unique_ptr<vector<Message>> dead_letters = move(dead_letter_office);
      dead_letter_office.reset(nullptr);

      return send_all(dead_letters->begin(), dead_letters->end());
    }
  }

  return ok_result(false);
}

void Thread::start()
{
  mark_running();

  // Artificially enqueue any messages that establish the thread's starting state.
  vector<Message> starter_messages = starter->get_messages();
  if (!starter_messages.empty()) {
    in.enqueue_all(starter_messages.begin(), starter_messages.end());
  }

  // Initialize any state necessary to call command handler methods.
  Result<> ir = init();
  if (ir.is_error()) {
    LOGGER << "Unable to initialize thread: " << ir << "." << endl;
  }

  // Handle any commands that were enqueued while the thread was starting.
  Result<size_t> cr = handle_commands();
  if (cr.is_error()) {
    LOGGER << "Unable to handle initially enqueued commands: " << cr << "." << endl;
  }

  Result<> r = body();
  if (r.is_error()) {
    LOGGER << "Thread stopping because of an error: " << r << "." << endl;
  } else {
    LOGGER << "Thread stopping normally." << endl;
  }

  Logger::disable();
  mark_stopped();
}

Result<> Thread::emit(Message &&message)
{
  out.enqueue(move(message));

  int uv_err = uv_async_send(main_callback);
  if (uv_err != 0) {
    return error_result(uv_strerror(uv_err));
  }

  return ok_result();
}

Result<Thread::OfflineCommandOutcome> Thread::handle_offline_command(const CommandPayload *payload)
{
  CommandAction action = payload->get_action();
  if (action == COMMAND_LOG_FILE || action == COMMAND_LOG_STDOUT || action == COMMAND_LOG_STDERR
    || action == COMMAND_LOG_DISABLE) {
    starter->set_logging(payload);
  }

  return ok_result(OFFLINE_ACK);
}

Result<size_t> Thread::handle_commands()
{
  unique_ptr<vector<Message>> accepted = in.accept_all();
  if (!accepted) {
    // No command messages to accept.
    return ok_result(static_cast<size_t>(0));
  }

  vector<Message> acks;
  acks.reserve(accepted->size());
  bool should_stop = false;

  for (Message &message : *accepted) {
    const CommandPayload *command = message.as_command();
    if (command == nullptr) {
      LOGGER << "Received unexpected non-command message " << message << "." << endl;
      continue;
    }

    CommandHandler handler = command_handlers[command->get_action()];
    if (handler == nullptr) {
      handler = &Thread::handle_unknown_command;
    }
    Result<CommandOutcome> hr = (this->*handler)(command);

    if (hr.is_error()) {
      acks.emplace_back(Message::ack(message, hr.propagate_as_void()));
    } else {
      CommandOutcome &outcome = hr.get_value();

      if (outcome == CommandOutcome::TRIGGER_STOP) {
        should_stop = true;
      }

      if (outcome == CommandOutcome::PREVENT_STOP) {
        should_stop = false;
      }

      if (outcome != CommandOutcome::NOTHING && command->get_id() != NULL_COMMAND_ID) {
        acks.emplace_back(Message::ack(message, hr.propagate_as_void()));
      }
    }
  }

  Result<> er = emit_all(acks.begin(), acks.end());
  if (er.is_error()) return er.propagate<size_t>();

  if (should_stop) {
    mark_stopping();

    // Move any messages enqueued since we picked up this batch of commands into the dead letter office.
    dead_letter_office = in.accept_all();

    // Notify the Hub if this thread has messages that need to be drained.
    if (dead_letter_office) {
      LOGGER << plural(dead_letter_office->size(), "message") << " are now waiting in the dead letter office." << endl;

      emit(Message(CommandPayloadBuilder::drain().build()));
    }
  }

  return ok_result(static_cast<size_t>(accepted->size()));
}

Result<Thread::CommandOutcome> Thread::handle_add_command(const CommandPayload *payload)
{
  return handle_unknown_command(payload);
}

Result<Thread::CommandOutcome> Thread::handle_remove_command(const CommandPayload *payload)
{
  return handle_unknown_command(payload);
}

Result<Thread::CommandOutcome> Thread::handle_log_file_command(const CommandPayload *payload)
{
  string err = Logger::to_file(payload->get_root().c_str());
  if (!err.empty()) return Result<CommandOutcome>::make_error(move(err));

  starter->set_logging(payload);
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> Thread::handle_log_stderr_command(const CommandPayload *payload)
{
  string err = Logger::to_stderr();
  if (!err.empty()) return Result<CommandOutcome>::make_error(move(err));

  starter->set_logging(payload);
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> Thread::handle_log_stdout_command(const CommandPayload *payload)
{
  string err = Logger::to_stdout();
  if (!err.empty()) return Result<CommandOutcome>::make_error(move(err));

  starter->set_logging(payload);
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> Thread::handle_log_disable_command(const CommandPayload *payload)
{
  string err = Logger::disable();
  if (!err.empty()) return Result<CommandOutcome>::make_error(move(err));

  starter->set_logging(payload);
  return ok_result(ACK);
}

Result<Thread::CommandOutcome> Thread::handle_polling_interval_command(const CommandPayload *payload)
{
  return handle_unknown_command(payload);
}

Result<Thread::CommandOutcome> Thread::handle_polling_throttle_command(const CommandPayload *payload)
{
  return handle_unknown_command(payload);
}

Result<Thread::CommandOutcome> Thread::handle_cache_size_command(const CommandPayload *payload)
{
  return handle_unknown_command(payload);
}

Result<Thread::CommandOutcome> Thread::handle_status_command(const CommandPayload *payload)
{
  return handle_unknown_command(payload);
}

Result<Thread::CommandOutcome> Thread::handle_unknown_command(const CommandPayload *payload)
{
  LOGGER << "Received command with unexpected action " << *payload << "." << endl;
  return ok_result(ACK);
}

string Thread::state_name()
{
  switch (state.load()) {
    case STOPPED: return "stopped";
    case STARTING: return "starting";
    case RUNNING: return "running";
    case STOPPING: return "stopping";
    default: return "!!";
  }
}

ostream &operator<<(ostream &out, const Thread &th)
{
  out << "Thread[" << th.name << "]";
  return out;
}
