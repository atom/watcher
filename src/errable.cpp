#include <string>
#include <utility>
#include <uv.h>

#include "errable.h"
#include "lock.h"

using std::string;
using std::move;

Errable::Errable() : healthy{true}, message{"ok"}
{
  //
}

bool Errable::is_healthy()
{
  return healthy;
}

void Errable::report_error(string &&message)
{
  healthy = false;
  this->message = move(message);
}

bool Errable::report_uv_error(int err_code)
{
  if (!err_code) {
    return false;
  }

  report_error(uv_strerror(err_code));
  return true;
}

string Errable::get_error() {
  return message;
}

SyncErrable::SyncErrable() : Errable()
{
  int err = uv_rwlock_init(&rwlock);

  if (err) {
    Errable::report_error(uv_strerror(err));
    lock_healthy = false;
  } else {
    lock_healthy = true;
  }
}

SyncErrable::~SyncErrable()
{
  uv_rwlock_destroy(&rwlock);
}

bool SyncErrable::is_healthy()
{
  if (!lock_healthy) {
    return false;
  }

  ReadLock lock(rwlock);
  return Errable::is_healthy();
}

void SyncErrable::report_error(string &&message)
{
  if (!lock_healthy) return;

  WriteLock lock(rwlock);
  Errable::report_error(move(message));
}

string SyncErrable::get_error()
{
  if (!lock_healthy) return Errable::get_error();

  ReadLock lock(rwlock);
  return Errable::get_error();
}
