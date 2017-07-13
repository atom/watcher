#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <memory>
#include <functional>
#include <uv.h>

#include "errable.h"

void thread_callback_helper(void *arg);

class ThreadCallback {
public:
  template< class T >
  ThreadCallback(T* self, void (T::*fn)()) :
    bound_fn{std::bind(std::mem_fn(fn), self)}
  {
    //
  };

  ~ThreadCallback() {};

  int create_thread(uv_thread_t *thread);

private:
  std::function<void()> bound_fn;
};

#endif
