#ifndef RECENT_FILE_CACHE_H
#define RECENT_FILE_CACHE_H

#include <chrono>
#include <dirent.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <utility>

#include "../../message.h"

class StatResult
{
public:
  static std::shared_ptr<StatResult> at(std::string &&path, bool file_hint, bool directory_hint);

  virtual bool is_present() const = 0;
  bool is_absent() const { return !is_present(); }

  virtual bool has_changed_from(const StatResult &other) const;
  virtual bool could_be_rename_of(const StatResult &other) const;

  const std::string &get_path() const;
  EntryKind get_entry_kind() const;

  virtual std::string to_string() const = 0;

protected:
  StatResult(std::string &&path, EntryKind entry_kind) : path{std::move(path)}, entry_kind{entry_kind} {};

private:
  std::string path;
  EntryKind entry_kind;
};

std::ostream &operator<<(std::ostream &out, const StatResult &result);

class PresentEntry : public StatResult
{
public:
  PresentEntry(std::string &&path, EntryKind entry_kind, ino_t inode, off_t size);

  bool is_present() const override;

  bool has_changed_from(const StatResult &other) const override;
  bool could_be_rename_of(const StatResult &other) const override;

  ino_t get_inode() const;
  off_t get_size() const;
  const std::chrono::time_point<std::chrono::steady_clock> &get_last_seen() const;

  std::string to_string() const override;

private:
  ino_t inode;
  off_t size;
  std::chrono::time_point<std::chrono::steady_clock> last_seen;
};

class AbsentEntry : public StatResult
{
public:
  AbsentEntry(std::string &&path, EntryKind entry_kind) : StatResult(std::move(path), entry_kind){};

  bool is_present() const override;

  bool has_changed_from(const StatResult &other) const override;
  bool could_be_rename_of(const StatResult &other) const override;

  std::string to_string() const override;
};

class RecentFileCache
{
public:
  void insert(const std::shared_ptr<StatResult> &stat_result);
  std::shared_ptr<StatResult> at_path(const std::string &path, bool file_hint, bool directory_hint);

  void prune();

  void prepopulate(const std::string &root, size_t max);

private:
  std::unordered_map<std::string, std::shared_ptr<PresentEntry>> by_path;
  std::multimap<std::chrono::time_point<std::chrono::steady_clock>, std::shared_ptr<PresentEntry>> by_timestamp;
};

#endif
