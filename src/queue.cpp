#include <iterator>
#include <vector>
#include <utility>
#include <uv.h>

#include "queue.h"
#include "event.h"
#include "lock.h"

using std::move;
using std::vector;
using std::back_inserter;
using std::copy;
using std::unique_ptr;

Queue::Queue() : Errable(), active{new vector<Event>}
{
  int err;

  err = uv_mutex_init(&mutex);
  report_uv_error(err);
}

Queue::~Queue()
{
  uv_mutex_destroy(&mutex);
}

template <class InputIt>
void Queue::enqueue_all(InputIt begin, InputIt end)
{
  if (!is_healthy()) return;

  Lock lock = {mutex};
  copy(begin, end, back_inserter(*active));
}

unique_ptr<vector<Event>> Queue::accept_all()
{
  if (!is_healthy()) return nullptr;

  Lock lock = {mutex};

  if (active->empty()) return nullptr;
  unique_ptr<vector<Event>> consumed = move(active);
  active.reset(new vector<Event>);

  return consumed;
}
