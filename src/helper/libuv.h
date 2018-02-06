#ifndef LIBUV_H
#define LIBUV_H

#include <iostream>
#include <uv.h>

#include "../message.h"

struct FSReq
{
  uv_fs_t req{};

  FSReq() = default;
  FSReq(const FSReq &) = delete;
  FSReq(FSReq &&) = delete;
  ~FSReq() { uv_fs_req_cleanup(&req); }

  FSReq &operator=(const FSReq &) = delete;
  FSReq &operator=(FSReq &&) = delete;
};

inline std::ostream &operator<<(std::ostream &out, const uv_timespec_t &ts)
{
  return out << ts.tv_sec << "s " << ts.tv_nsec << "ns";
}

std::ostream &operator<<(std::ostream &out, const uv_stat_t &stat);

std::ostream &operator<<(std::ostream &out, const FSReq &r);

inline bool ts_not_equal(const uv_timespec_t &left, const uv_timespec_t &right)
{
  return left.tv_sec != right.tv_sec || left.tv_nsec != right.tv_nsec;
}

inline EntryKind kind_from_stat(const uv_stat_t &st)
{
  if ((st.st_mode & S_IFLNK) == S_IFLNK) return KIND_SYMLINK;
  if ((st.st_mode & S_IFDIR) == S_IFDIR) return KIND_DIRECTORY;
  if ((st.st_mode & S_IFREG) == S_IFREG) return KIND_FILE;
  return KIND_UNKNOWN;
}

#endif
