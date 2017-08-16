#ifndef RESULT_H
#define RESULT_H

#include <utility>
#include <string>
#include <iostream>

#include "log.h"

template< class V = void* >
class Result {
public:
  static Result<V> make_ok(V &&value)
  {
    return Result<V>(std::move(value));
  }

  static Result<V> make_error(std::string &&message)
  {
    return Result<V>(std::move(message), true);
  }

  Result(Result<V> &&original) : state{original.state}, pending{false}
  {
    switch (state) {
      case OK: // wat
        new (&value) V(std::move(original.value));
        break;
      case ERROR:
        new (&error) std::string(std::move(original.error));
        break;
      default:
        LOGGER << "Invalid result state " << state << " in Result::Result(Result&&)." << std::endl;
        break;
    }
  }

  ~Result()
  {
    switch (state) {
      case OK:
        value.~V();
        break;
      case ERROR:
        error.~basic_string();
        break;
      default:
        LOGGER << "Invalid result state " << state << " in Result::~Result()." << std::endl;
        break;
    }
  }

  Result<V> &operator=(Result<V>&& original) = delete;
  Result<V> &operator=(const Result<V> &original) = delete;

  bool is_ok() const
  {
    return state == OK;
  }

  bool is_error() const
  {
    return state == ERROR;
  }

  V &get_value()
  {
    return value;
  }

  const std::string& get_error() const
  {
    return error;
  }

private:
  Result(V &&value) : state{OK}, value{std::move(value)}
  {
    //
  }

  Result(std::string &&error, bool ignored) : state{ERROR}, error{std::move(error)}
  {
    //
  }

  Result(const Result<V> &original) : state{original.state}, pending{false}
  {
    switch (state) {
      case OK:
        new (&value) V(original.value);
        break;
      case ERROR:
        new (&error) std::string(original.error);
        break;
      default:
        LOGGER << "Invalid result state " << state << " in Result::Result(Result&)" << std::endl;
        break;
    }
  }

  enum {
    OK = 0,
    ERROR
  } state;

  union {
    V value;
    std::string error;
    bool pending;
  };

  friend Result<void*> ok_result();
};

template < class V >
Result<V> ok_result(V &&value)
{
  return Result<V>::make_ok(std::move(value));
}

inline Result<void*> ok_result()
{
  return Result<void*>::make_ok(nullptr);
}

inline Result<void*> error_result(std::string &&message)
{
  return Result<void*>::make_error(std::move(message));
}

template < class V >
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
