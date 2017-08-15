#ifndef RESULT_H
#define RESULT_H

#include <utility>
#include <string>
#include <iostream>

template< class V = void* >
class Result {
public:
  static Result<V> &&make_ok(V &&value)
  {
    return std::move(Result<V>(std::move(value)));
  }

  static Result<V> &&make_error(std::string &&message)
  {
    return std::move(Result<V>(std::move(message), true));
  }

  Result(Result &&original) : state{original.state}, pending{true}
  {
    switch (state) {
      case OK:
        value = std::move(original.value);
        break;
      case ERROR:
        error = std::move(original.error);
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
    }
  }

  bool is_ok() const
  {
    return state == OK;
  }

  bool is_error() const
  {
    return state == ERROR;
  }

  V& get_value()
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

  Result(const Result<V> &other) : state{other.state}, pending{true}
  {
    switch (state) {
      case OK:
        value = other.value;
        break;
      case ERROR:
        error = other.error;
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

  friend Result<void*> &&ok_result();
};

template < class V >
Result<V> &&make_ok_result(V &&value)
{
  return std::move(Result<V>::make_ok(std::move(value)));
}

inline Result<void*> &&ok_result()
{
  return std::move(Result<void*>::make_ok(nullptr));
}

inline Result<void*> &&error_result(std::string &&message)
{
  return Result<void*>::make_error(std::move(message));
}

template < class V >
std::ostream &operator<<(std::ostream &out, const Result<V> &result)
{
  if (result.is_error()) {
    out << result.get_error();
  } else {
    out << "OK";
  }
  return out;
}

#endif
