#include <uv.h>

#include "lock.h"

Lock::Lock(uv_mutex_t &mutex) : mutex{mutex}
{
  uv_mutex_lock(&mutex);
}

Lock::~Lock()
{
  uv_mutex_unlock(&mutex);
}

ReadLock::ReadLock(uv_rwlock_t &rwlock) : rwlock{rwlock}
{
  uv_rwlock_rdlock(&rwlock);
}

ReadLock::~ReadLock()
{
  uv_rwlock_rdunlock(&rwlock);
}

WriteLock::WriteLock(uv_rwlock_t &rwlock) : rwlock{rwlock}
{
  uv_rwlock_wrlock(&rwlock);
}

WriteLock::~WriteLock()
{
  uv_rwlock_wrunlock(&rwlock);
}
