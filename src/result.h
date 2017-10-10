#ifndef RESULT_H
#define RESULT_H

#include <cassert>
#include <iostream>
#include <string>
#include <utility>

#include "log.h"

// Container to be returned from method calls that may fail, optionally wrapping a return value.
//
// Result objects are expected to be stack-allocated and returned using move semantics or return-value optimization.
// As a consequence, wrapped return values must have valid move constructors.
//
// To construct them, use the static {Result<>::make_ok} or {Result<>::make_error} methods:
//
// ```
// Result<int> r0 = Result<int>::make_ok(12);
//
// Result<int> r1 = Result<int>::make_error("something went wrong");
// ```
//
// For the common case of returning void, use the shorthand {ok_result()} and {error_result()} functions:
//
// ```
// Result<> r2 = ok_result();
//
// Result<> r3 = error_result("oh no");
// ```
//
// When returning a concrete value, the {ok_result()} override can also be useful for brevity:
//
// ```
// Result<int> r4 = ok_result(12);
// ```
//
// After construction, return them directly from a function, not by pointer, reference, or xvalue (`&&`):
//
// ```
// Result<int> get_value(bool succeed)
// {
//   if (succeed) {
//     return ok_result(12);
//   } else {
//     return Result<int>::make_error("oh no");
//   }
// }
// ```
//
// To consume a function that returns a Result, check for success with {is_error()} or {is_ok()}, then either return it
// up the stack, process the error message extracted with {get_error()}, or process the return value extracted with
// {get_value()}. Note that {get_value()} returns a reference.
//
// ```
// Result<int> stuff()
// {
//   Result<int> r = other_method();
//   if (r.is_error()) {
//     LOGGER << "The error was: " << r << std::endl;
//     return r;
//   }
//
//   int &foo = r.get_value();
//
//   return ok_result(foo + 10);
// }
// ```
template <class V = void *>
class Result
{
public:
  static Result<V> make_ok(V &&value) { return Result<V>(std::forward<V>(value)); }

  static Result<V> make_error(std::string &&message) { return Result<V>(std::move(message), true); }

  Result(Result<V> &&original) noexcept : state{original.state}, pending{false} { assign(std::move(original)); }

  ~Result() { clear(); }

  Result<V> &operator=(Result<V> &&original) noexcept
  {
    clear();
    assign(std::move(original));
    return *this;
  }

  Result<V> &operator=(const Result<V> &original) = delete;

  bool is_ok() const { return state == RESULT_OK; }

  bool is_error() const { return state == RESULT_ERROR; }

  V &get_value()
  {
    assert(state == RESULT_OK);
    return value;
  }

  const std::string &get_error() const
  {
    assert(state == RESULT_ERROR);
    return error;
  }

  template <class U = void *>
  Result<U> propagate(const std::string &prefix = "") const
  {
    assert(state == RESULT_ERROR);
    return Result<U>::make_error(prefix + get_error());
  }

  template <class U>
  Result<U> propagate(U &&value, const std::string &prefix = "") const
  {
    if (state == RESULT_ERROR) {
      return propagate<U>(prefix);
    }

    return Result<U>::make_ok(std::forward<U>(value));
  }

  Result<> propagate_as_void() const
  {
    if (state == RESULT_ERROR) {
      return propagate();
    }

    return Result<>::make_ok(nullptr);
  }

  Result<V> &operator&=(Result<V> &&sub_result)
  {
    if (is_error() != sub_result.is_error() || sub_result.is_ok()) {
      clear();
      assign(std::move(sub_result));
    } else if (is_error()) {
      error += ", " + sub_result.get_error();
    }

    return *this;
  }

private:
  Result(V &&value) : state{RESULT_OK}, value{std::move(value)}
  {
    //
  }

  Result(std::string &&error, bool /*ignored*/) : state{RESULT_ERROR}, error{std::move(error)}
  {
    //
  }

  Result(const Result<V> &original) : state{original.state}, pending{false}
  {
    switch (state) {
      case RESULT_OK: new (&value) V(original.value); break;
      case RESULT_ERROR: new (&error) std::string(original.error); break;
      default: LOGGER << "Invalid result state " << state << " in Result::Result(Result&)" << std::endl; break;
    }
  }

  void assign(Result &&original)
  {
    state = original.state;

    switch (state) {
      case RESULT_OK: new (&value) V(std::move(original.value)); break;
      case RESULT_ERROR: new (&error) std::string(std::move(original.error)); break;
      default: LOGGER << "Invalid result state " << state << " in Result::assign(Result&&)." << std::endl; break;
    }
  }

  void clear()
  {
    switch (state) {
      case RESULT_OK: value.~V(); break;
      case RESULT_ERROR: error.~basic_string(); break;
      default: LOGGER << "Invalid result state " << state << " in Result::clear()." << std::endl; break;
    }
  }

  enum
  {
    RESULT_OK = 0,
    RESULT_ERROR
  } state;

  union
  {
    V value;
    std::string error;
    bool pending{false};
  };
};

template <class V>
Result<V> ok_result(V &&value)
{
  return Result<V>::make_ok(std::forward<V>(value));
}

inline Result<void *> ok_result()
{
  return Result<void *>::make_ok(nullptr);
}

inline Result<void *> error_result(std::string &&message)
{
  return Result<void *>::make_error(std::move(message));
}

template <class V>
std::ostream &operator<<(std::ostream &out, const Result<V> &result)
{
  if (result.is_error()) {
    out << result.get_error();
  } else if (result.is_ok()) {
    out << "OK";
  } else {
    out << "INVALID";
  }
  return out;
}

#endif
