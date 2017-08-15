#include <iterator>
#include <vector>
#include <utility>
#include <string>
#include <uv.h>

#include "queue.h"
#include "message.h"
#include "lock.h"
#include "result.h"

using std::move;
using std::vector;
using std::back_inserter;
using std::copy;
using std::unique_ptr;
using std::string;

Queue::Queue(string name) : Errable(name), active{new vector<Message>}
{
  int err;

  err = uv_mutex_init(&mutex);
  if (err) {
    report_uv_error(err);
  }
}

Queue::~Queue()
{
  uv_mutex_destroy(&mutex);
}

Result<> &&Queue::enqueue(Message &&message)
{
  if (!is_healthy()) return health_err_result();

  Lock lock(mutex);
  active->push_back(move(message));
  return ok_result();
}

Result< unique_ptr<vector<Message>> > &&Queue::accept_all()
{
  if (!is_healthy()) return health_err_result< unique_ptr<vector<Message>> >();

  Lock lock = {mutex};

  if (active->empty()) {
    unique_ptr<vector<Message>> n = nullptr;
    return make_ok_result(move(n));
  }

  unique_ptr<vector<Message>> consumed = move(active);
  active.reset(new vector<Message>);

  return make_ok_result(move(consumed));
}

size_t Queue::size()
{
  Lock lock(mutex);
  return active->size();
}
