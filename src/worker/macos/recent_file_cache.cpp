#include "recent_file_cache.h"

#include <string>
#include <chrono>
#include <unordered_set>
#include <map>

using std::string;
using std::unordered_set;
using std::multimap;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::seconds;

void RecentFileCache::does_exist(const string &path, const EntryKind &entryKind)
{
  bool inserted = seen_paths.insert({path, entryKind}).second;
  if (inserted) {
    time_point<steady_clock> ts = steady_clock::now();
    by_timestamp.insert({ts, path});
  }
}

void RecentFileCache::does_not_exist(const string &path)
{
  seen_paths.erase(path);
}

bool RecentFileCache::has_been_seen(const string &path, const EntryKind &entryKind)
{
  auto maybe = seen_paths.find(path);
  if (maybe == seen_paths.end()) {
    return false;
  } else {
    return maybe->second == entryKind;
  }
}

EntryKind RecentFileCache::last_known_kind(const string &path)
{
  auto maybe_entry_kind = seen_paths.find(path);
  if (maybe_entry_kind == seen_paths.end()) {
    return KIND_UNKNOWN;
  }
  return maybe_entry_kind->second;
}

void RecentFileCache::purge()
{
  time_point<steady_clock> oldest = steady_clock::now() - seconds(5);

  auto to_keep = by_timestamp.upper_bound(oldest);
  for (auto it = by_timestamp.begin(); it != to_keep && it != by_timestamp.end(); ++it) {
    seen_paths.erase(it->second);
  }

  if (to_keep != by_timestamp.begin()) {
    by_timestamp.erase(by_timestamp.begin(), to_keep);
  }
}
