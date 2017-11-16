#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <uv.h>

#include "../helper/common.h"
#include "../helper/libuv.h"
#include "../log.h"
#include "../message.h"
#include "directory_record.h"
#include "polling_iterator.h"

using std::move;
using std::ostringstream;
using std::set;
using std::shared_ptr;
using std::string;

DirectoryRecord::DirectoryRecord(string &&prefix) :
  parent{nullptr},
  name{move(prefix)},
  populated{false},
  was_present{false}
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
    if (scan_err == UV_ENOENT || scan_err == UV_ENOTDIR || scan_err == UV_EACCES) {
      if (was_present) {
        entry_deleted(it, dir, KIND_DIRECTORY);
        was_present = false;
      }
    } else {
      ostringstream msg;
      msg << "Unable to scan directory " << dir << ": " << uv_strerror(scan_err);
      it->get_buffer().error(msg.str(), false);
    }

    return;
  }

  if (!was_present) {
    entry_created(it, dir, KIND_DIRECTORY);
    was_present = true;
  }

  uv_dirent_t dirent{};
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
    ostringstream msg;
    msg << "Unable to list entries in directory " << dir << ": " << uv_strerror(next_err);

    it->get_buffer().error(msg.str(), false);
  } else {
    // Report entries that were present the last time we scanned this directory, but aren't included in this
    // scan.
    auto previous = entries.begin();
    while (previous != entries.end()) {
      const string &previous_entry_name = previous->first;
      const string previous_entry_path(path_join(dir, previous_entry_name));
      EntryKind previous_entry_kind = kind_from_stat(previous->second);
      Entry previous_entry(previous_entry_name, previous_entry_kind);
      Entry unknown_entry(previous_entry_name, KIND_UNKNOWN);

      if (scanned_entries.count(previous_entry) == 0 && scanned_entries.count(unknown_entry) == 0) {
        entry_deleted(it, previous_entry_path, previous_entry_kind);
        auto former = previous;
        ++previous;

        subdirectories.erase(previous_entry_name);
        entries.erase(former);
      } else {
        ++previous;
      }
    }
  }
}

void DirectoryRecord::entry(BoundPollingIterator *it,
  const string &entry_name,
  const string &entry_path,
  EntryKind scan_kind)
{
  FSReq lstat_req;
  EntryKind previous_kind = scan_kind;
  EntryKind current_kind = scan_kind;

  int lstat_err = uv_fs_lstat(nullptr, &lstat_req.req, entry_path.c_str(), nullptr);
  if (lstat_err != 0 && lstat_err != UV_ENOENT && lstat_err != UV_EACCES) {
    ostringstream msg;
    msg << "Unable to stat " << entry_path << ": " << uv_strerror(lstat_err);
    it->get_buffer().error(msg.str(), false);
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
    if (kinds_are_different(previous_kind, current_kind) || previous_stat.st_ino != current_stat.st_ino) {
      entry_deleted(it, entry_path, previous_kind);
      entry_created(it, entry_path, current_kind);
    } else if (previous_stat.st_mode != current_stat.st_mode || previous_stat.st_size != current_stat.st_size
      || ts_not_equal(previous_stat.st_mtim, current_stat.st_mtim)
      || ts_not_equal(previous_stat.st_ctim, current_stat.st_ctim)) {
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
  if (current_kind == KIND_DIRECTORY && it->is_recursive()) {
    if (dir == subdirectories.end()) {
      shared_ptr<DirectoryRecord> subdir(new DirectoryRecord(this, string(entry_name)));
      subdirectories.emplace(entry_name, subdir);
      it->push_directory(subdir);
    } else {
      it->push_directory(dir->second);
    }
  }
}

bool DirectoryRecord::all_populated() const
{
  if (!populated) return false;

  for (auto &pair : subdirectories) {
    if (!pair.second->all_populated()) {
      return false;
    }
  }

  return true;
}

size_t DirectoryRecord::count_entries() const
{
  // Start with 1 to count the readdir() on this directory.
  size_t count = 1;
  for (auto &pair : entries) {
    if ((pair.second.st_mode & S_IFDIR) != S_IFDIR) {
      count++;
    }
  }
  for (auto &pair : subdirectories) {
    count += pair.second->count_entries();
  }
  return count;
}

DirectoryRecord::DirectoryRecord(DirectoryRecord *parent, string &&name) :
  parent{parent},
  name(move(name)),
  populated{false},
  was_present{false}
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
