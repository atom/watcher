#include <functional>
#include <uv.h>

#include "callbacks.h"

void thread_callback_helper(void *arg)
{
  std::function<void()> *bound_fn = (std::function<void()>*) arg;
  (*bound_fn)();
}

int ThreadCallback::create_thread(uv_thread_t *thread)
{
  return uv_thread_create(thread, thread_callback_helper, &bound_fn);
}
