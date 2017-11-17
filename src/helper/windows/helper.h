#ifndef HELPER_H
#define HELPER_H

#include <sstream>
#include <string>
#include <windows.h>

#include "../../result.h"

// Convert a wide-character string to a utf8 string.
Result<std::string> to_utf8(const std::wstring &in);

// Convert a utf8 string to a wide-character string.
Result<std::wstring> to_wchar(const std::string &in);

// Convert an 8.3 short path to a long path.
Result<std::wstring> to_long_path(const std::wstring &short_path);

template <class V = void *>
Result<V> windows_error_result(const std::string &prefix)
{
  return windows_error_result<V>(prefix, GetLastError());
}

template <class V = void *>
Result<V> windows_error_result(const std::string &prefix, DWORD error_code)
{
  LPVOID msg_buffer;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,  // source
    error_code,  // message ID
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // language ID
    (LPSTR) &msg_buffer,  // output buffer
    0,  // size
    NULL  // arguments
  );

  std::string msg_str(static_cast<char *>(msg_buffer));
  // Remove the pesky CRLF and punctuation
  if (msg_str.size() > 3) {
    msg_str.erase(msg_str.size() - 3, 3);
  }

  std::ostringstream msg;
  msg << prefix << " (" << error_code << ") " << msg_str;
  LocalFree(msg_buffer);

  return Result<V>::make_error(msg.str());
}

#endif
