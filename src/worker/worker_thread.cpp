#include <vector>
#include <memory>
#include <iostream>
#include <utility>
#include <string>
#include <uv.h>

#include "worker_thread.h"
#include "worker_platform.h"
#include "../log.h"
#include "../queue.h"
#include "../message.h"
#include "../result.h"

using std::endl;
using std::vector;
using std::unique_ptr;
using std::move;
using std::string;

WorkerThread::WorkerThread(uv_async_t *main_callback) :
  Thread(this, &WorkerThread::listen, "worker thread", main_callback),
  platform{WorkerPlatform::for_worker(this)}
{
  //
}

WorkerThread::~WorkerThread()
{
  // Necessary so that unique_ptr can see the full definition of WorkerPlatform
}

Result<> WorkerThread::wake()
{
  if (!is_healthy()) return health_err_result();

  std::cerr << "WorkerThread::wake() called" << endl;
  return platform->wake();
}

void WorkerThread::listen()
{
  std::cerr << "WorkerThread::listen() called" << endl;

  // Handle any commands that were enqueued while the thread was starting.
  Result<> cr = handle_commands();
  if (cr.is_error()) {
    LOGGER << "Unable to handle initially enqueued commands: " << cr << endl;
  }

  Result<> lr = platform->listen();
  if (lr.is_error()) {
    LOGGER << "Unable to listen: " << lr << endl;
    report_error(string(lr.get_error()));
  } else {
    report_error("WorkerPlatform::listen() returned unexpectedly");
    LOGGER << "listen unexpectedly returned without reporting an error." << endl;
  }

  std::cerr << "WorkerThread::listen() completed" << endl;
}

Result<> WorkerThread::handle_commands()
{
  if (!is_healthy()) return health_err_result();

  std::cerr << "WorkerThread::handle_commands() started" << endl;
  Result< unique_ptr<vector<Message>> > pr = process_all();
  if (pr.is_error()) {
    return error_result(string(pr.get_error()));
  }

  unique_ptr<vector<Message>> &accepted = pr.get_value();
  if (!accepted) {
    // No command messages to accept.
    return ok_result();
  }

  vector<Message> acks;
  acks.reserve(accepted->size());

  std::cerr << "WorkerThread::handle_commands() handling " << accepted->size() << " commands" << endl;

  for (auto it = accepted->begin(); it != accepted->end(); ++it) {
    const CommandPayload *command = it->as_command();
    if (!command) {
      LOGGER << "Received unexpected message " << *it << "." << endl;
      continue;
    }

    bool success = true;
    bool ack = true;
    string message = "";

    switch (command->get_action()) {
      case COMMAND_ADD:
        {
          Result<bool> r = platform->handle_add_command(
            command->get_id(),
            command->get_channel_id(),
            command->get_root());
          if (r.is_error()) {
            success = false;
            message = r.get_error();
          } else {
            ack = r.get_value();
          }
        }
        break;

      case COMMAND_REMOVE:
        {
          Result<bool> r = platform->handle_remove_command(
            command->get_id(),
            command->get_channel_id());
          if (r.is_error()) {
            success = false;
            message = r.get_error();
          } else {
            ack = r.get_value();
          }
        }
        break;

      case COMMAND_LOG_FILE:
        Logger::to_file(command->get_root().c_str());
        break;

      case COMMAND_LOG_DISABLE:
        LOGGER << "Disabling logger." << endl;
        Logger::disable();
        break;

      default:
        LOGGER << "Received command with unexpected action " << *it << "." << endl;
        break;
    }

    if (!message.empty()) {
      LOGGER << "Reporting platform error: " << message << "." << endl;
    }

    if (ack) {
      AckPayload ack(command->get_id(), command->get_channel_id(), success, move(message));
      Message response(move(ack));
      acks.push_back(move(response));
    }
  }

  return emit_all(acks.begin(), acks.end());
}

void WorkerThread::collect_status(Status &status)
{
  status.worker_thread_ok = get_error();
  status.worker_in_size = get_in_queue_size();
  status.worker_in_ok = get_in_queue_error();
  status.worker_out_size = get_out_queue_size();
  status.worker_out_ok = get_out_queue_error();
}
