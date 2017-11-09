#include <iterator>
#include <string>
#include <utility>
#include <uv.h>
#include <vector>

#include "lock.h"
#include "message.h"
#include "queue.h"
#include "result.h"

using std::move;
using std::string;
using std::unique_ptr;
using std::vector;

Queue::Queue() : active{new vector<Message>}
{
  int err;

  err = uv_mutex_init(&mutex);
  if (err != 0) {
    report_uv_error(err);
  }
  freeze();
}

Queue::~Queue()
{
  uv_mutex_destroy(&mutex);
}

void Queue::enqueue(Message &&message)
{
  Lock lock(mutex);
  active->push_back(move(message));
}

unique_ptr<vector<Message>> Queue::accept_all()
{
  Lock lock(mutex);

  if (active->empty()) {
    unique_ptr<vector<Message>> n;
    return n;
  }

  unique_ptr<vector<Message>> consumed = move(active);
  active.reset(new vector<Message>);
  return consumed;
}

size_t Queue::size()
{
  Lock lock(mutex);
  return active->size();
}
