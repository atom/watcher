#include <uv.h>

#include "libuv.h"
#include <iomanip>
#include <iostream>
#include <sys/stat.h>

using std::dec;
using std::hex;
using std::ostream;

ostream &operator<<(ostream &out, const uv_stat_t &stat)
{
  out << "[ino=" << stat.st_ino << " size=" << stat.st_size << " mode=" << hex << stat.st_mode << dec << " (";
  if ((stat.st_mode & S_IFDIR) == S_IFDIR) out << " DIR";
  if ((stat.st_mode & S_IFREG) == S_IFREG) out << " REG";
  if ((stat.st_mode & S_IFLNK) == S_IFLNK) out << " LNK";
  out << " ) atim=" << stat.st_atim << " mtim=" << stat.st_mtim << " birthtim=" << stat.st_birthtim << "]";
  return out;
}

ostream &operator<<(ostream &out, const FSReq &r)
{
  if (r.req.result < 0) {
    return out << "[" << uv_strerror(static_cast<int>(r.req.result)) << "]";
  }

  return out << r.req.statbuf;
}
