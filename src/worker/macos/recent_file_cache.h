#ifndef RECENT_FILE_CACHE_H
#define RECENT_FILE_CACHE_H

#include <string>
#include <chrono>
#include <unordered_map>
#include <map>

#include "../../message.h"

class RecentFileCache {
public:
  void does_exist(const std::string &path, const EntryKind &entryKind);
  void does_not_exist(const std::string &path);

  bool has_been_seen(const std::string &path, const EntryKind &entryKind);
  EntryKind last_known_kind(const std::string &path);

  void purge();
private:
  std::unordered_map<std::string, EntryKind> seen_paths;
  std::multimap<std::chrono::time_point<std::chrono::steady_clock>, std::string> by_timestamp;
};

#endif
