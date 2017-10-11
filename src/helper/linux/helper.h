#ifndef HELPER_H
#define HELPER_H

#include <cerrno>
#include <cstring>

#include <sstream>
#include <string>

#include "../../result.h"

inline int _strerror_result(char *buffer, char *&out, int r)
{
  // XSI strerror_r
  out = buffer;
  return r;
}

inline int _strerror_result(char * /*buffer*/, char *&out, char *r)
{
  // GNU strerror_r
  out = r;
  return 0;
}

template <class V = void *>
Result<V> errno_result(const std::string &prefix);

template <class V = void *>
Result<V> errno_result(const std::string &prefix, int errnum)
{
  const size_t BUFSIZE = 1024;
  char buffer[BUFSIZE];
  char *msg = buffer;

  // Use a function overloading trick to work with either strerror_r variant.
  // See https://linux.die.net/man/3/strerror_r for the different signatures.
  int result = _strerror_result(buffer, msg, strerror_r(errnum, buffer, BUFSIZE));
  if (result == EINVAL) {
    strncpy(msg, "Not a valid error number", BUFSIZE);
  } else if (result == ERANGE) {
    strncpy(msg, "Insuffient buffer size for error message", BUFSIZE);
  } else if (result < 0) {
    return errno_result<V>(prefix);
  }

  std::ostringstream out;
  out << prefix << " (" << errnum << ") " << msg;
  return Result<V>::make_error(out.str());
}

template <class V>
Result<V> errno_result(const std::string &prefix)
{
  return errno_result(prefix, errno);
}

#endif
