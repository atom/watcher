#ifndef ERRABLE_H
#define ERRABLE_H

#include <iostream>
#include <string>
#include <utility>
#include <uv.h>

#include "result.h"

// Superclass for resources that can potentially enter an errored state.
//
// Resources begin "healthy". If an operation necessary for the continued use of the
// resource is unsuccessful (a file can't be opened, a thread can't be started), one of
// the report_error() methods should be called with a message describing the failure
// state.
//
// All methods on the subclass should verify that is_health() returns true before
// attempting to take any actions, and return early otherwise, indicating failure.
//
// External consumers of the resource can use get_error() to log the cause of the failure.
class Errable
{
public:
  explicit Errable(std::string &&source);
  Errable(const Errable &) = delete;
  Errable(Errable &&) = delete;
  virtual ~Errable() = default;

  virtual bool is_healthy();
  virtual void report_error(std::string &&message);

  template <class V = void *>
  void report_error(const Result<V> &result)
  {
    report_error(std::string(result.get_error()));
  }

  void report_uv_error(int err_code);

  virtual std::string get_error();

  // Generate a Result from the current error status of this resource. If it has entered an error state,
  // an errored Result will be created with its error message. Otherwise, an ok Result will be regurned.
  template <class V = void *>
  Result<V> health_err_result()
  {
    std::string m = get_error();
    return Result<V>::make_error(std::move(m));
  }

  const std::string &get_source() const { return source; }

  Errable &operator=(const Errable &) = delete;
  Errable &operator=(Errable &&) = delete;

private:
  bool healthy;
  std::string source;
  std::string message;
};

// Thread-safe superclass for resources that can enter an errored state.
class SyncErrable : public Errable
{
public:
  explicit SyncErrable(std::string &&source);
  SyncErrable(const SyncErrable &) = delete;
  SyncErrable(SyncErrable &&) = delete;
  ~SyncErrable() override;

  bool is_healthy() override;
  void report_error(std::string &&message) override;
  std::string get_error() override;

  SyncErrable &operator=(const SyncErrable &) = delete;
  SyncErrable &operator=(SyncErrable &&) = delete;

private:
  bool lock_healthy;
  uv_rwlock_t rwlock{};
};

#endif
