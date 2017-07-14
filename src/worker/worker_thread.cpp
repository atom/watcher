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

#include <iomanip>

WorkerThread::WorkerThread(uv_async_t *main_callback) :
  Thread(this, &WorkerThread::listen),
  main_callback{main_callback},
  platform{WorkerPlatform::for_worker(this)}
{
  std::cout << "WorkerThread::WorkerThread constructor" << std::endl;
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
  std::cout << "In WorkerThread::listen()" << endl;

  std::cout << "Let's access this to see if it blows up:" << endl;
  wat();
  this->wat();

  std::cout << "Before handle_commands() call" << endl;

  // Handle any commands that were enqueued while the thread was starting.
  handle_commands();

  std::cout << "After handle::commands()" << endl;

  platform->listen();

  std::cout << "After platform::listen()" << endl;
}

void WorkerThread::handle_commands()
{
  LOGGER << "Handling command messages from the main thread." << endl;

  unique_ptr<vector<Message>> accepted = process_all();
  if (!accepted) {
    LOGGER << "No messages waiting." << endl;
    return;
  }

  LOGGER << accepted->size() << " message(s) to process." << endl;
  vector<Message> acks;
  acks.reserve(accepted->size());

  for (auto it = accepted->begin(); it != accepted->end(); ++it) {
    LOGGER << "Processing: " << *it << endl;
    const CommandPayload *command = it->as_command();
    if (!command) {
      LOGGER << "Received unexpected message " << *it << "." << endl;
      continue;
    }

    switch (command->get_action()) {
      case COMMAND_ADD:
        platform->handle_add_command(command->get_root());
        break;
      case COMMAND_REMOVE:
        platform->handle_remove_command(command->get_root());
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

    AckPayload ack(command->get_id());
    Message response(move(ack));
    LOGGER << "Ack produced: " << response << endl;
    acks.push_back(move(response));
  }

  LOGGER << "Replying with " << acks.size() << " ack(s)." << endl;
  emit_all(acks.begin(), acks.end());
  LOGGER << "Reply sent." << endl;
  uv_async_send(main_callback);
  LOGGER << "Main thread handler scheduled." << endl;
}
