#ifndef RECENT_FILE_CACHE_H
#define RECENT_FILE_CACHE_H

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <uv.h>

#include "../helper/libuv.h"
#include "../message.h"

class StatResult
{
public:
  static std::shared_ptr<StatResult> at(std::string &&path, bool file_hint, bool directory_hint, bool symlink_hint);

  virtual ~StatResult() = default;

  virtual bool is_present() const = 0;

  bool is_absent() const { return !is_present(); }

  virtual bool has_changed_from(const StatResult &other) const;

  virtual bool could_be_rename_of(const StatResult &other) const;

  bool update_for_rename(const std::string &from_dir_path, const std::string &to_dir_path);

  const std::string &get_path() const;

  EntryKind get_entry_kind() const;

  virtual std::string to_string(bool verbose = false) const = 0;

  StatResult(const StatResult &) = delete;
  StatResult(StatResult &&) = delete;
  StatResult &operator=(const StatResult &) = delete;
  StatResult &operator=(StatResult &&) = delete;

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
  PresentEntry(std::string &&path, EntryKind entry_kind, uint64_t inode, uint64_t size);

  ~PresentEntry() override = default;

  bool is_present() const override;

  bool has_changed_from(const StatResult &other) const override;

  bool could_be_rename_of(const StatResult &other) const override;

  uint64_t get_inode() const;

  uint64_t get_size() const;

  const std::chrono::time_point<std::chrono::steady_clock> &get_last_seen() const;

  std::string to_string(bool verbose = false) const override;

  PresentEntry(const PresentEntry &) = delete;
  PresentEntry(PresentEntry &&) = delete;
  PresentEntry &operator=(const PresentEntry &) = delete;
  PresentEntry &operator=(PresentEntry &&) = delete;

private:
  uint64_t inode;
  uint64_t size;
  std::chrono::time_point<std::chrono::steady_clock> last_seen;
};

class AbsentEntry : public StatResult
{
public:
  AbsentEntry(std::string &&path, EntryKind entry_kind) : StatResult(std::move(path), entry_kind){};

  ~AbsentEntry() override = default;

  bool is_present() const override;

  bool has_changed_from(const StatResult &other) const override;

  bool could_be_rename_of(const StatResult &other) const override;

  std::string to_string(bool verbose = false) const override;

  AbsentEntry(const AbsentEntry &) = delete;
  AbsentEntry(AbsentEntry &&) = delete;
  AbsentEntry &operator=(const AbsentEntry &) = delete;
  AbsentEntry &operator=(AbsentEntry &&) = delete;
};

class RecentFileCache
{
public:
  explicit RecentFileCache(size_t maximum_size);

  ~RecentFileCache() = default;

  std::shared_ptr<StatResult> current_at_path(const std::string &path,
    bool file_hint,
    bool directory_hint,
    bool symlink_hint);

  std::shared_ptr<StatResult> former_at_path(const std::string &path,
    bool file_hint,
    bool directory_hint,
    bool symlink_hint);

  void evict(const std::string &path);

  void evict(const std::shared_ptr<PresentEntry> &entry);

  void update_for_rename(const std::string &from_dir_path, const std::string &to_dir_path);

  void apply();

  void prune();

  void prepopulate(const std::string &root, size_t max, bool recursive);

  void resize(size_t maximum_size);

  size_t size() { return by_path.size(); }

  RecentFileCache(const RecentFileCache &) = delete;
  RecentFileCache(RecentFileCache &&) = delete;
  RecentFileCache &operator=(const RecentFileCache &) = delete;
  RecentFileCache &operator=(RecentFileCache &&) = delete;

private:
  size_t prepopulate_helper(const std::string &root, size_t max, bool recursive);

  size_t maximum_size;

  std::map<std::string, std::shared_ptr<PresentEntry>> pending;

  std::unordered_map<std::string, std::shared_ptr<PresentEntry>> by_path;

  std::multimap<std::chrono::time_point<std::chrono::steady_clock>, std::shared_ptr<PresentEntry>> by_timestamp;
};

#endif
