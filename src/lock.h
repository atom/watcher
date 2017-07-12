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

#endif
