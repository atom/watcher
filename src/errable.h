#ifndef ERRABLE_H
#define ERRABLE_H

#include <string>

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
class Errable {
public:
  Errable();

  bool is_healthy();
  void report_error(std::string &&message);
  bool report_uv_error(int errCode);
  std::string get_error();

private:
  bool healthy;
  std::string message;
};

#endif
