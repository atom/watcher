#ifndef QUEUE_H
#define QUEUE_H

#include <memory>
#include <vector>
#include <uv.h>

#include "event.h"
#include "errable.h"

// Primary channel of communication between threads.
//
// The producing thread accumulates a sequence of Events to be handled through repeated
// calls to .enqueue_all(). The consumer processes a chunk of Events by calling
// .accept_all().
class Queue : public Errable {
public:
  Queue();
  ~Queue();

  // Atomically enqueue a collection of Events from a source STL container type between
  // the iterators [begin, end).
  template <class InputIt>
  void enqueue_all(InputIt begin, InputIt end);

  // Atomically consume the current contents of the queue, emptying it.
  //
  // Returns a unique_ptr to the vector of Events, or nullptr if no Events were
  // present.
  std::unique_ptr<std::vector<Event>> accept_all();

private:
  uv_mutex_t mutex;
  std::unique_ptr<std::vector<Event>> active;
};

#endif
