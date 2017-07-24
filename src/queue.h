#ifndef QUEUE_H
#define QUEUE_H

#include <algorithm>
#include <memory>
#include <vector>
#include <iterator>
#include <uv.h>

#include "lock.h"
#include "message.h"
#include "errable.h"

// Primary channel of communication between threads.
//
// The producing thread accumulates a sequence of Messages to be handled through repeated
// calls to .enqueue_all(). The consumer processes a chunk of Messages by calling
// .accept_all().
class Queue : public Errable {
public:
  Queue();
  ~Queue();

  // Atomically enqueue a single Message.
  void enqueue(Message &&message);

  // Atomically enqueue a collection of Messages from a source STL container type between
  // the iterators [begin, end).
  template <class InputIt>
  void enqueue_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return;

    Lock lock(mutex);
    std::move(begin, end, std::back_inserter(*active));
  }

  // Atomically consume the current contents of the queue, emptying it.
  //
  // Returns a unique_ptr to the vector of Messages, or nullptr if no Messages were
  // present.
  std::unique_ptr<std::vector<Message>> accept_all();

  // Atomically report the number of items waiting on the queue.
  size_t size();

private:
  uv_mutex_t mutex;
  std::unique_ptr<std::vector<Message>> active;
};

#endif
