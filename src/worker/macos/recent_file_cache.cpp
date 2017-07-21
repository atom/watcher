#include "recent_file_cache.h"

#include <string>
#include <chrono>
#include <unordered_map>
#include <map>
#include <memory>
#include <utility>
#include <sys/stat.h>

#include "../../log.h"

using std::string;
using std::endl;
using std::move;
using std::shared_ptr;
using std::multimap;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::chrono::minutes;

// If the cache contains more than this many entries, any entries older than CACHE_AGEOFF will be purged.
static const size_t CACHE_WATERMARK = 4096;

// Entries older than this duration will be purged once the cache grows beyond CACHE_WATERMARK entries.
static const minutes CACHE_AGEOFF = minutes(5);

CacheEntry::CacheEntry(std::string path, EntryKind entry_kind, ino_t inode, off_t size) :
  path{path},
  entry_kind{entry_kind},
  inode{inode},
  size{size},
  last_seen{steady_clock::now()}
{
  //
}

void RecentFileCache::does_exist(const string &path, EntryKind entry_kind, ino_t inode, off_t size)
{
  shared_ptr<CacheEntry> entry(new CacheEntry(path, entry_kind, inode, size));

  // Clear an existing entry if one exists
  does_not_exist(path);

  by_path.insert({path, entry});
  by_inode.insert({inode, entry});
  by_timestamp.insert({entry->last_seen, entry});
}

void RecentFileCache::does_not_exist(const string &path)
{
  auto maybe = by_path.find(path);
  if (maybe != by_path.end()) {
    shared_ptr<CacheEntry> existing = maybe->second;

    by_inode.erase(existing->inode);
    by_timestamp.erase(existing->last_seen);
    by_path.erase(maybe);
  }
}

shared_ptr<CacheEntry> RecentFileCache::at_path(const string &path)
{
  auto maybe = by_path.find(path);
  return maybe != by_path.end() ? maybe->second : nullptr;
}

shared_ptr<CacheEntry> RecentFileCache::at_inode(const ino_t inode)
{
  auto maybe = by_inode.find(inode);
  return maybe != by_inode.end() ? maybe->second : nullptr;
}

void RecentFileCache::purge()
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
    shared_ptr<CacheEntry> entry = it->second;
    by_path.erase(entry->path);
    by_inode.erase(entry->inode);

    prune_count++;
  }

  if (to_keep != by_timestamp.begin()) {
    by_timestamp.erase(by_timestamp.begin(), to_keep);
  }

  LOGGER << "Pruned " << plural(prune_count, "entry", "entries") << ". "
    << plural(by_path.size(), "entry", "entries") << " remain." << endl;
}
