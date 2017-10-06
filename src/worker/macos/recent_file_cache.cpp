#include "recent_file_cache.h"

#include <string>
#include <chrono>
#include <unordered_map>
#include <map>
#include <queue>
#include <memory>
#include <utility>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <errno.h>

#include "../../helper/common.h"
#include "../../log.h"

using std::string;
using std::endl;
using std::move;
using std::shared_ptr;
using std::static_pointer_cast;
using std::multimap;
using std::queue;
using std::ostream;
using std::ostringstream;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::chrono::minutes;

// If the cache contains more than this many entries, any entries older than CACHE_AGEOFF will be purged.
static const size_t CACHE_WATERMARK = 4096;

// Entries older than this duration will be purged once the cache grows beyond CACHE_WATERMARK entries.
static const minutes CACHE_AGEOFF = minutes(5);

shared_ptr<StatResult> StatResult::at(const string &path, bool file_hint, bool directory_hint)
{
  struct stat path_stat;

  if (lstat(path.c_str(), &path_stat) != 0) {
    errno_t stat_errno = errno;

    // Ignore lstat() errors on entries that:
    // (a) we aren't allowed to see
    // (b) are at paths with too many symlinks or looping symlinks
    // (c) have names that are too long
    // (d) have a path component that is (no longer) a directory
    // Log any other errno that we see.
    if (stat_errno != ENOENT &&
        stat_errno != EACCES &&
        stat_errno != ELOOP &&
        stat_errno != ENAMETOOLONG &&
        stat_errno != ENOTDIR) {
      LOGGER << "lstat(" << path << ") failed with errno " << stat_errno << "." << endl;
    }

    EntryKind guessed_kind = KIND_UNKNOWN;
    if (file_hint && !directory_hint) guessed_kind = KIND_FILE;
    if (!file_hint && directory_hint) guessed_kind = KIND_DIRECTORY;

    return shared_ptr<StatResult>(new AbsentEntry(path, guessed_kind));
  }

  // Derive the entry kind from the lstat() results.
  EntryKind entry_kind = KIND_UNKNOWN;
  if ((path_stat.st_mode & S_IFREG) != 0) {
    entry_kind = KIND_FILE;
  }
  if ((path_stat.st_mode & S_IFDIR) != 0) {
    entry_kind = KIND_DIRECTORY;
  }

  return shared_ptr<StatResult>(new PresentEntry(path, entry_kind, path_stat.st_ino, path_stat.st_size));
}

bool StatResult::has_changed_from(const StatResult &other) const
{
  return entry_kind != other.entry_kind || path != other.path;
}

bool StatResult::could_be_rename_of(const StatResult &other) const
{
  return !kinds_are_different(entry_kind, other.entry_kind);
}

const string &StatResult::get_path() const
{
  return path;
}

EntryKind StatResult::get_entry_kind() const
{
  return entry_kind;
}

ostream &operator<<(ostream &out, const StatResult &result)
{
  out << result.to_string();
  return out;
}

PresentEntry::PresentEntry(const std::string &path, EntryKind entry_kind, ino_t inode, off_t size) :
  StatResult(path, entry_kind),
  inode{inode},
  size{size},
  last_seen{steady_clock::now()}
{
  //
}

bool PresentEntry::is_present() const
{
  return true;
}

bool PresentEntry::has_changed_from(const StatResult &other) const
{
  if (StatResult::has_changed_from(other)) return true;
  if (other.is_absent()) return true;

  const PresentEntry &casted = static_cast<const PresentEntry&>(other);

  return inode != casted.get_inode() || get_path() != casted.get_path();
}

bool PresentEntry::could_be_rename_of(const StatResult &other) const
{
  if (!StatResult::could_be_rename_of(other)) return false;
  if (other.is_absent()) return false;

  const PresentEntry &casted = static_cast<const PresentEntry&>(other);

  return inode == casted.get_inode();
}

ino_t PresentEntry::get_inode() const
{
   return inode;
}

off_t PresentEntry::get_size() const
{
  return size;
}

const time_point<steady_clock> &PresentEntry::get_last_seen() const
{
  return last_seen;
}

string PresentEntry::to_string() const
{
  ostringstream result;

  result << "[present "
    << get_entry_kind()
    << " ("
    << get_path()
    << ") inode " << inode
    << " size " << size
    << "]";

  return result.str();
}

bool AbsentEntry::is_present() const
{
  return false;
}

bool AbsentEntry::has_changed_from(const StatResult &other) const
{
  if (StatResult::has_changed_from(other)) return true;
  if (other.is_present()) return true;

  return false;
}

bool AbsentEntry::could_be_rename_of(const StatResult &other) const
{
  if (!StatResult::could_be_rename_of(other)) return false;
  if (other.is_present()) return false;

  return true;
}

string AbsentEntry::to_string() const
{
  ostringstream result;

  result
    << "[absent "
    << get_entry_kind()
    << " ("
    << get_path()
    << ")]";

  return result.str();
}

void RecentFileCache::insert(shared_ptr<StatResult> stat_result)
{
  // Clear an existing entry at the same path if one exists
  auto maybe = by_path.find(stat_result->get_path());
  if (maybe != by_path.end()) {
    shared_ptr<PresentEntry> existing = maybe->second;

    auto range = by_timestamp.equal_range(existing->get_last_seen());
    auto to_erase = by_timestamp.end();
    for (auto it = range.first; it != range.second; ++it) {
      if (it->second == existing) {
        to_erase = it;
      }
    }
    if (to_erase != by_timestamp.end()) {
      by_timestamp.erase(to_erase);
    }

    by_path.erase(maybe);
  }

  // Add the new result if it's a PresentEntry
  if (stat_result->is_present()) {
    shared_ptr<PresentEntry> present = static_pointer_cast<PresentEntry>(stat_result);

    by_path.insert({present->get_path(), present});
    by_timestamp.insert({present->get_last_seen(), present});
  }
}

shared_ptr<StatResult> RecentFileCache::at_path(const string &path, bool file_hint, bool directory_hint)
{
  auto maybe = by_path.find(path);
  if (maybe == by_path.end()) {
    EntryKind kind = KIND_UNKNOWN;
    if (file_hint && !directory_hint) kind = KIND_FILE;
    if (!file_hint && directory_hint) kind = KIND_DIRECTORY;

    return shared_ptr<StatResult>(new AbsentEntry(path, kind));
  } else {
    return maybe->second;
  }
}

void RecentFileCache::prune()
{
  if (by_path.size() <= CACHE_WATERMARK) {
    return;
  }

  LOGGER << "Cache currently contains " << plural(by_path.size(), "entry", "entries")
    << ". Pruning triggered." << endl;

  time_point<steady_clock> oldest = steady_clock::now() - CACHE_AGEOFF;

  auto to_keep = by_timestamp.upper_bound(oldest);
  size_t prune_count = 0;
  for (auto it = by_timestamp.begin(); it != to_keep && it != by_timestamp.end(); ++it) {
    shared_ptr<PresentEntry> entry = it->second;
    by_path.erase(entry->get_path());

    prune_count++;
  }

  if (to_keep != by_timestamp.begin()) {
    by_timestamp.erase(by_timestamp.begin(), to_keep);
  }

  LOGGER << "Pruned " << plural(prune_count, "entry", "entries") << ". "
    << plural(by_path.size(), "entry", "entries") << " remain." << endl;
}

void RecentFileCache::prepopulate(const string &root, size_t count)
{
  queue<string> next_roots;
  next_roots.push(root);

  while (count > 0 && !next_roots.empty()) {
    string current_root(next_roots.front());
    next_roots.pop();

    DIR *dir = opendir(current_root.c_str());
    if (!dir) {
      errno_t opendir_errno = errno;
      LOGGER << "Unable to open directory " << root << ": " << opendir_errno << "." << endl;
      return;
    }

    errno = 0;
    dirent *entry = readdir(dir);
    while (entry != NULL) {
      string entry_name(entry->d_name, entry->d_namlen);
      string entry_path(path_join(root, entry_name));

      bool file_hint = (entry->d_type & DT_REG) == DT_REG;
      bool dir_hint = (entry->d_type & DT_DIR) == DT_DIR;

      shared_ptr<StatResult> r = StatResult::at(entry_path, file_hint, dir_hint);
      if (r->is_present()) {
        insert(r);

        if (r->get_entry_kind() == KIND_DIRECTORY) {
          next_roots.push(entry_path);
        }
      }

      count--;
      if (count <= 0) return;

      errno = 0;
      entry = readdir(dir);
    }
    errno_t readdir_errno = errno;
    if (readdir_errno) {
      LOGGER << "Unable to read directory entry within " << root << ": " << readdir_errno << "." << endl;
    }

    if (closedir(dir)) {
      errno_t closedir_errno = errno;
      LOGGER << "Unable to close directory " << root << ": " << closedir_errno << "." << endl;
    }
  }
}
