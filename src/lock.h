#ifndef LOCK_H
#define LOCK_H

#include <uv.h>

// Hold a UV mutex for the lifetime of the Lock instance.
// RAII FTW
class Lock {
public:
  Lock(uv_mutex_t &mutex);
  ~Lock();

private:
  uv_mutex_t &mutex;
};

// Hold a read lock on a uv_rwlock_t for the lifetime of this instance.
class ReadLock {
public:
  ReadLock(uv_rwlock_t &rwlock);
  ~ReadLock();

private:
  uv_rwlock_t &rwlock;
};

// Hold a write lock on a uv_rwlock_t for the lifetime of this instance.
class WriteLock {
public:
  WriteLock(uv_rwlock_t &rwlock);
  ~WriteLock();

private:
  uv_rwlock_t &rwlock;
};

#endif
