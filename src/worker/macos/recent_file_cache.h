#ifndef RECENT_FILE_CACHE_H
#define RECENT_FILE_CACHE_H

#include <string>
#include <chrono>
#include <unordered_map>
#include <map>
#include <memory>
#include <sys/stat.h>

#include "../../message.h"

struct CacheEntry {
  CacheEntry(std::string path, EntryKind entry_kind, ino_t inode, off_t size);

  std::string path;
  EntryKind entry_kind;
  ino_t inode;
  off_t size;
  std::chrono::time_point<std::chrono::steady_clock> last_seen;
};

class RecentFileCache {
public:
  void does_exist(const std::string &path, EntryKind entry_kind, ino_t inode, off_t size);
  void does_not_exist(const std::string &path);

  std::shared_ptr<CacheEntry> at_path(const std::string &path);
  std::shared_ptr<CacheEntry> at_inode(const ino_t inode);

  void purge();
private:
  std::unordered_map<std::string, std::shared_ptr<CacheEntry>> by_path;
  std::unordered_map<ino_t, std::shared_ptr<CacheEntry>> by_inode;
  std::multimap<std::chrono::time_point<std::chrono::steady_clock>, std::shared_ptr<CacheEntry>> by_timestamp;
};

#endif
