#ifndef QUEUE_H
#define QUEUE_H

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <uv.h>
#include <vector>

#include "errable.h"
#include "lock.h"
#include "message.h"
#include "result.h"

// Primary channel of communication between threads.
//
// The producing thread accumulates a sequence of Messages to be handled through repeated
// calls to .enqueue_all(). The consumer processes a chunk of Messages by calling
// .accept_all().
class Queue : public Errable
{
public:
  Queue();

  ~Queue() override;

  // Atomically enqueue a single Message.
  void enqueue(Message &&message);

  // Atomically enqueue a collection of Messages from a source STL container type between
  // the iterators [begin, end).
  template <class InputIt>
  void enqueue_all(InputIt begin, InputIt end)
  {
    Lock lock(mutex);
    std::move(begin, end, std::back_inserter(*active));
  }

  // Atomically consume the current contents of the queue, emptying it.
  //
  // Returns a result containing unique_ptr to the vector of Messages, nullptr if no Messages were
  // present, or an error if the Queue is unhealthy.
  std::unique_ptr<std::vector<Message>> accept_all();

  // Atomically report the number of items waiting on the queue.
  size_t size();

  Queue(const Queue &) = delete;
  Queue(Queue &&) = delete;
  Queue &operator=(const Queue &) = delete;
  Queue &operator=(Queue &&) = delete;

private:
  uv_mutex_t mutex{};
  std::unique_ptr<std::vector<Message>> active;
};

#endif
