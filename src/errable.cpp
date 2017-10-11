#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <uv.h>

#include "errable.h"
#include "lock.h"

using std::move;
using std::ostream;
using std::string;

Errable::Errable(string &&source) : healthy{true}, source{move(source)}, message{"ok"}
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

void Errable::report_uv_error(int err_code)
{
  report_error(uv_strerror(err_code));
}

string Errable::get_error()
{
  return message;
}

SyncErrable::SyncErrable(string &&source) : Errable(move(source))
{
  int err = uv_rwlock_init(&rwlock);

  if (err != 0) {
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
