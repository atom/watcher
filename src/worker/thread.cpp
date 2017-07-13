#include <vector>
#include <memory>
#include <iostream>
#include <utility>
#include <uv.h>

#include "thread.h"
#include "../log.h"
#include "../queue.h"
#include "../event.h"

using std::endl;
using std::vector;
using std::unique_ptr;
using std::move;

WorkerThread::WorkerThread(Queue &in, Queue &out, uv_async_t *main_callback) :
  in{in},
  out{out},
  main_callback{main_callback},
  start_callback{this, &WorkerThread::work}
{
  //
}

void WorkerThread::run()
{
  int err;

  err = start_callback.create_thread(&thread);
  report_uv_error(err);
}

void WorkerThread::work()
{
  Logger::toFile("worker.log");

  LOGGER << "What's up" << endl;
}

void WorkerThread::handle_events()
{
  LOGGER << "Handling events." << endl;

  unique_ptr<vector<Event>> accepted = in.accept_all();
  if (!accepted) {
    LOGGER << "No events waiting." << endl;
    return;
  }

  LOGGER << "Handling " << accepted->size() << " events." << endl;
  vector<Event> acks;
  acks.reserve(accepted->size());

  for (auto it = accepted->begin(); it != accepted->end(); ++it) {
    CommandEvent *command = it->as_command();
    if (!command) {
      LOGGER << "Received unexpected event " << *it << "." << endl;
      continue;
    }

    switch (command->get_action()) {
      case COMMAND_ADD:
        LOGGER << "Adding watcher for path " << command->get_root() << "." << endl;
        break;
      case COMMAND_REMOVE:
        LOGGER << "Removing watcher for path " << command->get_root() << "." << endl;
        break;
      case COMMAND_LOG_FILE:
        Logger::toFile(command->get_root().c_str());
        break;
      case COMMAND_LOG_DISABLE:
        LOGGER << "Disabling logger." << endl;
        Logger::disable();
        break;
      default:
        LOGGER << "Received command with unexpected action " << *it << "." << endl;
        break;
    }

    AckEvent ack(&*it);
    Event response(move(ack));
    acks.push_back(move(response));
  }

  LOGGER << "Replying with " << acks.size() << " acks." << endl;
  out.enqueue_all(acks.begin(), acks.end());
  uv_async_send(main_callback);
}
