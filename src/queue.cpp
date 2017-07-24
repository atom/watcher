#include <iterator>
#include <vector>
#include <utility>
#include <uv.h>

#include "queue.h"
#include "message.h"
#include "lock.h"

using std::move;
using std::vector;
using std::back_inserter;
using std::copy;
using std::unique_ptr;

Queue::Queue() : Errable(), active{new vector<Message>}
{
  int err;

  err = uv_mutex_init(&mutex);
  report_uv_error(err);
}

Queue::~Queue()
{
  uv_mutex_destroy(&mutex);
}

void Queue::enqueue(Message &&message)
{
  if (!is_healthy()) return;

  Lock lock(mutex);
  active->push_back(move(message));
}

unique_ptr<vector<Message>> Queue::accept_all()
{
  if (!is_healthy()) return nullptr;

  Lock lock = {mutex};

  if (active->empty()) return nullptr;
  unique_ptr<vector<Message>> consumed = move(active);
  active.reset(new vector<Message>);

  return consumed;
}

size_t Queue::size()
{
  Lock lock(mutex);
  return active->size();
}
