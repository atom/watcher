#include <uv.h>

#include "lock.h"

Lock::Lock(uv_mutex_t &mutex)
  : mutex{mutex}
{
  uv_mutex_lock(&mutex);
}

Lock::~Lock()
{
  uv_mutex_unlock(&mutex);
}
