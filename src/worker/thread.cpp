#include <vector>
#include <memory>
#include <iostream>
#include <utility>
#include <uv.h>

#include "thread.h"
#include "platform.h"
#include "../log.h"
#include "../queue.h"
#include "../message.h"

using std::endl;
using std::vector;
using std::unique_ptr;
using std::move;

WorkerThread::WorkerThread(Queue &in, Queue &out, uv_async_t *main_callback) :
  in{in},
  out{out},
  main_callback{main_callback},
  start_callback{this, &WorkerThread::listen},
  platform{WorkerPlatform::for_worker(this)}
{
  //
}

WorkerThread::~WorkerThread()
{
  delete platform;
}

void WorkerThread::run()
{
  int err;

  err = start_callback.create_thread(&thread);
  report_uv_error(err);
}

void WorkerThread::wake()
{
  platform->wake();
}

void WorkerThread::listen()
{
  platform->listen();
}

void WorkerThread::handle_commands()
{
  LOGGER << "Handling events." << endl;

  unique_ptr<vector<Message>> accepted = in.accept_all();
  if (!accepted) {
    LOGGER << "No events waiting." << endl;
    return;
  }

  LOGGER << "Handling " << accepted->size() << " events." << endl;
  vector<Message> acks;
  acks.reserve(accepted->size());

  for (auto it = accepted->begin(); it != accepted->end(); ++it) {
    const CommandPayload *command = it->as_command();
    if (!command) {
      LOGGER << "Received unexpected event " << *it << "." << endl;
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

    AckPayload ack(&*it);
    Message response(move(ack));
    acks.push_back(move(response));
  }

  LOGGER << "Replying with " << acks.size() << " acks." << endl;
  out.enqueue_all(acks.begin(), acks.end());
  uv_async_send(main_callback);
}
