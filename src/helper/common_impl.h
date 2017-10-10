#ifndef COMMON_IMPL_H
#define COMMON_IMPL_H

#include <string>

using std::string;
using std::wstring;

template <class Str>
Str _path_join_impl(const Str &left, const Str &right, const typename Str::value_type &sep)
{
  Str joined(left);

  if (left.back() != sep && right.front() != sep) {
    joined.reserve(left.size() + right.size() + 1);
    joined += sep;
  } else {
    joined.reserve(left.size() + right.size());
  }

  joined += right;

  return joined;
}

string path_join(const string &left, const string &right)  // NOLINT
{
  return _path_join_impl<string>(left, right, DIRECTORY_SEPARATOR);
}

wstring wpath_join(const wstring &left, const wstring &right)  // NOLINT
{
  return _path_join_impl<wstring>(left, right, W_DIRECTORY_SEPARATOR);
}

#endif
