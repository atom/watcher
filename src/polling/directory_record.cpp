#include <map>
#include <string>
#include <set>
#include <memory>
#include <utility>
#include <iostream>
#include <iomanip>
#include <uv.h>

#include "../log.h"
#include "../message.h"
#include "../helper/common.h"
#include "polling_iterator.h"
#include "directory_record.h"

using std::string;
using std::set;
using std::endl;
using std::hex;
using std::dec;
using std::ostream;
using std::move;
using std::shared_ptr;

struct FSReq {
  uv_fs_t req;

  ~FSReq() {
    uv_fs_req_cleanup(&req);
  }
};

ostream &operator<<(ostream &out, const uv_timespec_t &ts)
{
  return out
     << ts.tv_sec << "s "
     << ts.tv_nsec << "ns";
}

ostream &operator<<(ostream &out, const uv_stat_t &stat)
{
  out
    << "[ino=" << stat.st_ino
    << " size=" << stat.st_size
    << " mode=" << hex << stat.st_mode << dec << " (";
  if (stat.st_mode & S_IFDIR) out << " DIR";
  if (stat.st_mode & S_IFREG) out << " REG";
  if ((stat.st_mode & S_IFLNK) == S_IFLNK) out << " LNK";
  out
    << " ) atim=" << stat.st_atim
    << " mtim=" << stat.st_mtim
    << " birthtim=" << stat.st_birthtim
    << "]";
  return out;
}

ostream &operator<<(ostream &out, const FSReq &r)
{
  if (r.req.result < 0) {
    return out << "[" << uv_strerror(r.req.result) << "]";
  }

  return out << r.req.statbuf;
}

inline bool ts_less_than(const uv_timespec_t &left, const uv_timespec_t &right)
{
  return left.tv_sec < right.tv_sec ||
    (left.tv_sec == right.tv_sec && left.tv_nsec < right.tv_nsec);
}

inline EntryKind kind_from_stat(const uv_stat_t &st)
{
  if (st.st_mode & S_IFDIR) return KIND_DIRECTORY;
  if (st.st_mode & S_IFREG) return KIND_FILE;
  return KIND_UNKNOWN;
}

DirectoryRecord::DirectoryRecord(string &&name) :
  parent{nullptr},
  name{move(name)},
  populated{false}
{
  //
}

string DirectoryRecord::path() const
{
  return parent == nullptr ? name : path_join(parent->path(), name);
}

void DirectoryRecord::scan(BoundPollingIterator *it)
{
  FSReq scan_req;
  set<Entry> scanned_entries;

  string dir = path();
  int scan_err = uv_fs_scandir(nullptr, &scan_req.req, dir.c_str(), 0, nullptr);
  if (scan_err < 0) {
    LOGGER << "Unable to scan directory " << dir << ": " << uv_strerror(scan_err) << "." << endl;
    return;
  }

  uv_dirent_t dirent;
  int next_err = uv_fs_scandir_next(&scan_req.req, &dirent);
  while (next_err == 0) {
    string entry_name(dirent.name);

    EntryKind entry_kind = KIND_UNKNOWN;
    if (dirent.type == UV_DIRENT_FILE) entry_kind = KIND_FILE;
    if (dirent.type == UV_DIRENT_DIR) entry_kind = KIND_DIRECTORY;

    it->push_entry(string(entry_name), entry_kind);
    if (populated) scanned_entries.emplace(move(entry_name), entry_kind);

    next_err = uv_fs_scandir_next(&scan_req.req, &dirent);
  }

  if (next_err != UV_EOF) {
    LOGGER << "Unable to list entries in directory " << dir << ": " << uv_strerror(next_err) << "." << endl;
  } else {
    // Report entries that were present the last time we scanned this directory, but aren't included in this
    // scan.
    auto previous = entries.begin();
    while (previous != entries.end()) {
      const string &previous_entry_name = previous->first;
      const string previous_entry_path(path_join(dir, previous_entry_name));
      EntryKind previous_entry_kind = kind_from_stat(previous->second);
      Entry previous_entry(previous_entry_name, previous_entry_kind);

      if (scanned_entries.count(previous_entry) == 0) {
        entry_deleted(it, previous_entry_path, previous_entry_kind);
        auto former = previous;
        ++previous;
        entries.erase(former);
      } else {
        ++previous;
      }
    }
  }
}

void DirectoryRecord::entry(
  BoundPollingIterator *it,
  const string &entry_name,
  const string &entry_path,
  EntryKind scan_kind
) {
  FSReq lstat_req;
  EntryKind previous_kind = scan_kind;
  EntryKind current_kind = scan_kind;

  int lstat_err = uv_fs_lstat(nullptr, &lstat_req.req, entry_path.c_str(), nullptr);
  if (lstat_err != 0 && lstat_err != UV_ENOENT && lstat_err != UV_EACCES) {
    LOGGER << "Unable to stat " << entry_path << ": " << uv_strerror(lstat_err) << "." << endl;
  }

  auto previous = entries.find(entry_name);

  bool existed_before = previous != entries.end();
  bool exists_now = lstat_err == 0;

  if (existed_before) previous_kind = kind_from_stat(previous->second);
  if (exists_now) current_kind = kind_from_stat(lstat_req.req.statbuf);

  ostream &logline = LOGGER << entry_path << ":\n  ";
  if (existed_before) {
    logline << previous->second;
  } else {
    logline << "(missing)";
  }
  logline << "\n  ==>\n  "
    << lstat_req
    << endl;

  if (existed_before && exists_now) {
    // Modification or no change
    uv_stat_t &previous_stat = previous->second;
    uv_stat_t &current_stat = lstat_req.req.statbuf;

    // TODO consider modifications to mode or ownership bits?
    if (kinds_are_different(previous_kind, current_kind)) {
      entry_deleted(it, entry_path, previous_kind);
      entry_created(it, entry_path, current_kind);
    } else if (
      previous_stat.st_mode != current_stat.st_mode ||
      previous_stat.st_ino != current_stat.st_ino ||
      ts_less_than(previous_stat.st_mtim, current_stat.st_mtim) ||
      ts_less_than(previous_stat.st_ctim, current_stat.st_ctim)
    ) {
      entry_modified(it, entry_path, current_kind);
    }

  } else if (existed_before && !exists_now) {
    // Deletion

    entry_deleted(it, entry_path, previous_kind);

  } else if (!existed_before && exists_now) {
    // Creation

    if (kinds_are_different(scan_kind, current_kind)) {
      // Entry was created as a file, deleted, then recreated as a directory between scan() and entry()
      // (or vice versa)
      entry_created(it, entry_path, scan_kind);
      entry_deleted(it, entry_path, scan_kind);
    }
    entry_created(it, entry_path, current_kind);

  } else if (!existed_before && !exists_now) {
    // Entry was deleted between scan() and entry().
    // Emit a deletion and creation event pair. Note that the kinds will likely both be KIND_UNKNOWN.

    entry_created(it, entry_path, previous_kind);
    entry_deleted(it, entry_path, current_kind);

  }

  // Update entries with the latest stat information
  if (existed_before) entries.erase(previous);
  if (exists_now) entries.emplace(entry_name, lstat_req.req.statbuf);

  // Update subdirectories if this is or was a subdirectory
  auto dir = subdirectories.find(entry_name);
  if (current_kind != KIND_DIRECTORY && current_kind != KIND_UNKNOWN && dir != subdirectories.end()) {
    subdirectories.erase(dir);
  }
  if (current_kind == KIND_DIRECTORY) {
    if (dir == subdirectories.end()) {
      shared_ptr<DirectoryRecord> subdir(new DirectoryRecord(this, entry_name));
      subdirectories.emplace(entry_name, subdir);
      it->push_directory(subdir);
    } else {
      it->push_directory(dir->second);
    }
  }
}

DirectoryRecord::DirectoryRecord(DirectoryRecord *parent, const string &name) :
  parent{parent},
  name(move(name)),
  populated{false}
{
  //
}

void DirectoryRecord::entry_deleted(BoundPollingIterator *it, const string &entry_path, EntryKind kind)
{
  if (!populated) return;

  it->get_buffer().deleted(string(entry_path), kind);
}

void DirectoryRecord::entry_created(BoundPollingIterator *it, const string &entry_path, EntryKind kind)
{
  if (!populated) return;

  it->get_buffer().created(string(entry_path), kind);
}

void DirectoryRecord::entry_modified(BoundPollingIterator *it, const string &entry_path, EntryKind kind)
{
  if (!populated) return;

  it->get_buffer().modified(string(entry_path), kind);
}
