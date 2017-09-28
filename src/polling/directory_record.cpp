#include <map>
#include <string>
#include <set>
#include <memory>
#include <utility>
#include <iostream>
#include <uv.h>

#include "../log.h"
#include "../message.h"
#include "../helper/common.h"
#include "polling_iterator.h"
#include "directory_record.h"

using std::string;
using std::set;
using std::endl;
using std::move;
using std::shared_ptr;

struct FSReq {
  uv_fs_t req;

  ~FSReq() {
    uv_fs_req_cleanup(&req);
  }
};

inline bool ts_less_than(const uv_timespec_t &left, const uv_timespec_t &right)
{
  return left.tv_sec < right.tv_sec ||
    (left.tv_sec == right.tv_sec && left.tv_nsec < right.tv_nsec);
}

inline EntryKind kind_from_stat(const uv_stat_t &st)
{
  if (st.st_flags & S_IFDIR) return KIND_DIRECTORY;
  if (st.st_flags & S_IFREG) return KIND_FILE;
  return KIND_UNKNOWN;
}

DirectoryRecord::DirectoryRecord(string &&name) :
  parent{nullptr},
  name{move(name)},
  populated{false}
{
  //
}

string DirectoryRecord::path()
{
  return parent == nullptr ? name : path_join(parent->path(), name);
}

void DirectoryRecord::scan(BoundPollingIterator *it)
{
  FSReq scan_req;
  set<string> scanned_entries;

  string dir = path();
  int scan_err = uv_fs_scandir(nullptr, &scan_req.req, dir.c_str(), 0, nullptr);
  if (scan_err != 0) {
    LOGGER << "Unable to scan directory " << dir << ": " << uv_strerror(scan_err) << "." << endl;
    return;
  }

  uv_dirent_t dirent;
  int next_err = uv_fs_scandir_next(&scan_req.req, &dirent);
  while (next_err == 0) {
    string entry_name(dirent.name);

    it->push_entry(entry_name);
    scanned_entries.emplace(move(entry_name));

    next_err = uv_fs_scandir_next(&scan_req.req, &dirent);
  }

  if (next_err != UV_EOF) {
    LOGGER << "Unable to list entries in directory " << dir << ": " << uv_strerror(next_err) << "." << endl;
  }
}

void DirectoryRecord::entry(BoundPollingIterator *it, const string &entry_name)
{
  FSReq lstat_req;
  // TODO cache path()
  string entry_path = path_join(path(), entry_name);
  EntryKind previous_kind = KIND_UNKNOWN;
  EntryKind current_kind = KIND_UNKNOWN;

  int lstat_err = uv_fs_lstat(nullptr, &lstat_req.req, entry_path.c_str(), nullptr);
  if (lstat_err != 0 && lstat_err != UV_ENOENT && lstat_err != UV_EACCES) {
    LOGGER << "Unable to stat " << entry_path << ": " << uv_strerror(lstat_err) << "." << endl;
  }

  auto previous = entries.find(entry_name);

  bool existed_before = previous != entries.end();
  bool exists_now = lstat_err == 0;

  if (existed_before) previous_kind = kind_from_stat(previous->second);
  if (exists_now) current_kind = kind_from_stat(lstat_req.req.statbuf);

  if (existed_before && exists_now) {
    // Modification or no change
    uv_stat_t &previous_stat = previous->second;
    uv_stat_t &current_stat = lstat_req.req.statbuf;

    // TODO consider modifications to mode or ownership bits?
    if (
      previous_stat.st_flags != current_stat.st_flags ||
      previous_stat.st_ino != current_stat.st_ino ||
      ts_less_than(previous_stat.st_mtim, current_stat.st_mtim) ||
      ts_less_than(previous_stat.st_ctim, current_stat.st_ctim)
    ) entry_modified(it, entry_path, current_kind);

  } else if (existed_before && !exists_now) {
    // Deletion

    entry_deleted(it, entry_path, previous_kind);

  } else if (!existed_before && exists_now) {
    // Creation

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
