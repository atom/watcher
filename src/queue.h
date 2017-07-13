#ifndef QUEUE_H
#define QUEUE_H

#include <memory>
#include <vector>
#include <uv.h>

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

  // Atomically enqueue a collection of Messages from a source STL container type between
  // the iterators [begin, end).
  template <class InputIt>
  void enqueue_all(InputIt begin, InputIt end);

  // Atomically consume the current contents of the queue, emptying it.
  //
  // Returns a unique_ptr to the vector of Messages, or nullptr if no Messages were
  // present.
  std::unique_ptr<std::vector<Message>> accept_all();

private:
  uv_mutex_t mutex;
  std::unique_ptr<std::vector<Message>> active;
};

#endif
