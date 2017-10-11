#include <memory>
#include <string>
#include <windows.h>

#include "helper.h"

using std::string;
using std::unique_ptr;
using std::wstring;

Result<string> to_utf8(const wstring &in)
{
  size_t len = WideCharToMultiByte(CP_UTF8,  // code page
    0,  // flags
    in.data(),  // source string
    in.size(),  // source string length
    nullptr,  // destination string, null to measure
    0,  // destination string length
    nullptr,  // default char
    nullptr  // used default char
  );
  if (!len) {
    return windows_error_result<string>("Unable to measure string as UTF-8");
  }

  unique_ptr<char[]> payload(new char[len]);
  size_t copied = WideCharToMultiByte(CP_UTF8,  // code page
    0,  // flags
    in.data(),  // source string
    in.size(),  // source string length
    payload.get(),  // destination string
    len,  // destination string length
    nullptr,  // default char
    nullptr  // used default char
  );
  if (!copied) {
    return windows_error_result<string>("Unable to convert string to UTF-8");
  }

  return ok_result(string(payload.get(), len));
}

Result<wstring> to_wchar(const string &in)
{
  size_t wlen = MultiByteToWideChar(CP_UTF8,  // code page
    0,  // flags
    in.c_str(),  // source string data
    -1,  // source string length (null-terminated)
    0,  // output buffer
    0  // output buffer size
  );
  if (wlen == 0) {
    return windows_error_result<wstring>("Unable to measure string as wide string");
  }

  unique_ptr<WCHAR[]> payload(new WCHAR[wlen]);
  size_t conv_success = MultiByteToWideChar(CP_UTF8,  // code page
    0,  // flags,
    in.c_str(),  // source string data
    -1,  // source string length (null-terminated)
    payload.get(),  // output buffer
    wlen  // output buffer size (in bytes)
  );
  if (!conv_success) {
    return windows_error_result<wstring>("Unable to convert string to wide string");
  }

  return ok_result(wstring(payload.get(), wlen - 1));
}
