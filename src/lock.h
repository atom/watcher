#ifndef LOCK_H
#define LOCK_H

#include <uv.h>

// Hold a UV mutex for the lifetime of the Lock instance.
// RAII FTW
class Lock
{
public:
  Lock(uv_mutex_t &mutex);
  Lock(const Lock &) = delete;
  Lock(Lock &&) = delete;
  ~Lock();

  Lock &operator=(const Lock &) = delete;
  Lock &operator=(Lock &&) = delete;

private:
  uv_mutex_t &mutex;
};

// Hold a read lock on a uv_rwlock_t for the lifetime of this instance.
class ReadLock
{
public:
  ReadLock(uv_rwlock_t &rwlock);
  ReadLock(const ReadLock &) = delete;
  ReadLock(ReadLock &&) = delete;
  ~ReadLock();

  ReadLock &operator=(const ReadLock &) = delete;
  ReadLock &operator=(ReadLock &&) = delete;

private:
  uv_rwlock_t &rwlock;
};

// Hold a write lock on a uv_rwlock_t for the lifetime of this instance.
class WriteLock
{
public:
  WriteLock(uv_rwlock_t &rwlock);
  WriteLock(const WriteLock &) = delete;
  WriteLock(WriteLock &&) = delete;
  ~WriteLock();

  WriteLock &operator=(const WriteLock &) = delete;
  WriteLock &operator=(WriteLock &&) = delete;

private:
  uv_rwlock_t &rwlock;
};

#endif
