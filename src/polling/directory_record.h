#ifndef DIRECTORY_RECORD_H
#define DIRECTORY_RECORD_H

#include <map>
#include <string>
#include <memory>
#include <iostream>
#include <uv.h>

#include "../message.h"

class BoundPollingIterator;

class DirectoryRecord {
public:
  DirectoryRecord(std::string &&name);
  DirectoryRecord(const DirectoryRecord &) = delete;
  DirectoryRecord(DirectoryRecord &&) = delete;
  ~DirectoryRecord() = default;

  std::string path() const;

  void scan(BoundPollingIterator *iterator);
  void entry(
    BoundPollingIterator *iterator,
    const std::string &entry_name,
    const std::string &entry_path,
    EntryKind scan_kind
  );
  void mark_populated() { populated = true; }

  DirectoryRecord &operator=(const DirectoryRecord &) = delete;
  DirectoryRecord &operator=(DirectoryRecord &&) = delete;
private:
  DirectoryRecord(DirectoryRecord *parent, const std::string &name);

  void entry_deleted(BoundPollingIterator *iterator, const std::string &entry_path, EntryKind kind);
  void entry_created(BoundPollingIterator *iterator, const std::string &entry_path, EntryKind kind);
  void entry_modified(BoundPollingIterator *iterator, const std::string &entry_path, EntryKind kind);

  DirectoryRecord *parent;
  std::string name;

  std::map<std::string, std::shared_ptr<DirectoryRecord>> subdirectories;
  std::map<std::string, uv_stat_t> entries;
  bool populated;

  friend std::ostream &operator<<(std::ostream &out, const DirectoryRecord &record)
  {
    out << "DirectoryRecord{" << record.name
      << " entries=" << record.entries.size()
      << " subdirectories=" << record.subdirectories.size();
    if (record.populated) out << " populated";
    return out << "}";
  }
};

#endif