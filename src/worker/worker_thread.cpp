#include <vector>
#include <memory>
#include <iostream>
#include <utility>
#include <uv.h>

#include "worker_thread.h"
#include "platform.h"
#include "../log.h"
#include "../queue.h"
#include "../message.h"

using std::endl;
using std::vector;
using std::unique_ptr;
using std::move;

WorkerThread::WorkerThread(uv_async_t *main_callback) :
  Thread(this, &WorkerThread::listen, main_callback),
  platform{WorkerPlatform::for_worker(this)}
{
  //
}

WorkerThread::~WorkerThread()
{
  // Necessary so that unique_ptr can see the full definition of WorkerPlatform
}

void WorkerThread::wake()
{
  platform->wake();
}

void WorkerThread::listen()
{
  // Handle any commands that were enqueued while the thread was starting.
  handle_commands();

  platform->listen();
}

void WorkerThread::handle_commands()
{
  unique_ptr<vector<Message>> accepted = process_all();
  if (!accepted) {
    return;
  }

  vector<Message> acks;
  acks.reserve(accepted->size());

  for (auto it = accepted->begin(); it != accepted->end(); ++it) {
    const CommandPayload *command = it->as_command();
    if (!command) {
      LOGGER << "Received unexpected message " << *it << "." << endl;
      continue;
    }

    switch (command->get_action()) {
      case COMMAND_ADD:
        platform->handle_add_command(command->get_channel_id(), command->get_root());
        break;
      case COMMAND_REMOVE:
        platform->handle_remove_command(command->get_channel_id());
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

    AckPayload ack(command->get_id(), command->get_channel_id(), true, "");
    Message response(move(ack));
    acks.push_back(move(response));
  }

  emit_all(acks.begin(), acks.end());
}
