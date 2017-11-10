#ifndef ERRABLE_H
#define ERRABLE_H

#include <iostream>
#include <string>
#include <utility>
#include <uv.h>

#include "result.h"

// Superclass for resources that can potentially fail to be constructed properly.
//
// While a resource is being constructed, if a required resource cannot be initialized correctly, call one of the
// report_error() functions to mark it as "unhealthy". Before exiting the constructor, call freeze() to prevent further
// modifications.
//
// External consumers of the resource can use health_err_result() to log the cause of the failure.
class Errable
{
public:
  Errable() = default;

  virtual ~Errable() = default;

  bool is_healthy() const { return message.empty(); }

  std::string get_message() const { return message.empty() ? "ok" : message; }

  // Generate a Result from the current error status of this resource. If it has entered an error state,
  // an errored Result will be created with its error message. Otherwise, an ok Result will be returned.
  Result<> health_err_result() const;

  Errable(const Errable &) = delete;
  Errable(Errable &&) = delete;
  Errable &operator=(const Errable &) = delete;
  Errable &operator=(Errable &&) = delete;

protected:
  void report_errable(const Errable &component);

  void report_uv_error(int err_code);

  void report_error(std::string &&message);

  template <class V = void *>
  void report_if_error(const Result<V> &result)
  {
    assert(!frozen);

    if (result.is_ok()) return;
    report_error(std::string(result.get_error()));
  }

  void freeze() { frozen = true; }

private:
  bool frozen{false};
  std::string message;
};

#endif
