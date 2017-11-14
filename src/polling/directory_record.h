#ifndef DIRECTORY_RECORD_H
#define DIRECTORY_RECORD_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <uv.h>

#include "../message.h"

class BoundPollingIterator;

// Remembered stat() results from the previous time a polling cycle visited a subdirectory of a `PolledRoot`. Contains
// a recursive substructure that mirrors the last-known state of the filesystem tree.
class DirectoryRecord
{
public:
  // Create a new, unpopulated directory with no parent. `prefix` should be a fully qualified path to the directory
  // tree.
  //
  // The record begins in an unpopulated state, which means that the first scan will discover existing entries, but
  // not emit any events.
  DirectoryRecord(std::string &&prefix);

  DirectoryRecord(const DirectoryRecord &) = delete;
  DirectoryRecord(DirectoryRecord &&) = delete;
  ~DirectoryRecord() = default;
  DirectoryRecord &operator=(const DirectoryRecord &) = delete;
  DirectoryRecord &operator=(DirectoryRecord &&) = delete;

  // Access the full path of this directory by walking up the `DirectoryRecord` tree.
  //
  // This is reasonably expensive on deep filesystems, so you should probably cache it somewhere.
  std::string path() const;

  // Perform a `scandir()` on this directories. If populated, emit deletion events for any entries that were found here
  // before but are now missing. Store the discovered entries within `it` as part of the iteration state.
  void scan(BoundPollingIterator *it);

  // Perform a single `lstat()` on an entry within this directory. If the DirectoryRecord is populated and the entry
  // has been created, deleted, or modified since the previous `DirectoryRecord::entry()` call, emit the appropriate
  // events into the `it`'s buffer.
  void entry(BoundPollingIterator *it,
    const std::string &entry_name,
    const std::string &entry_path,
    EntryKind scan_kind);

  // Note that this `DirectoryResult` has had an initial `scan()` and set of `entry()` calls completed. Subsequent
  // calls should emit actual events.
  void mark_populated() { populated = true; }

  // Return true if all `DirectoryResults` beneath this one have been populated by an initial scan.
  bool all_populated() const;

  // Recursively count the number of stat entries tracked beneath this directory, including this directory itself, as
  // of the last scan.
  size_t count_entries() const;

private:
  // Construct a `DirectoryRecord` for a child entry.
  DirectoryRecord(DirectoryRecord *parent, std::string &&name);

  // Use an iterator to emit deletion, creation, or modification events.
  void entry_deleted(BoundPollingIterator *it, const std::string &entry_path, EntryKind kind);
  void entry_created(BoundPollingIterator *it, const std::string &entry_path, EntryKind kind);
  void entry_modified(BoundPollingIterator *it, const std::string &entry_path, EntryKind kind);

  // The parent directory. May be `null` at the root `DirectoryRecord` of a subtree.
  DirectoryRecord *parent;

  // If `parent` is null, this contains the full directory prefix. Otherwise, it contains only the entry name of this
  // directory in its parent.
  std::string name;

  // Recursive subdirectory records.
  std::map<std::string, std::shared_ptr<DirectoryRecord>> subdirectories;

  // Recorded stat results from previous scans. Includes stat results for *all* entries within the directory that are
  // not `.` or `..`.
  std::map<std::string, uv_stat_t> entries;

  // If true, a complete pass has already filled `entries` and `subdirectories` with initial stat results to compare
  // against. Otherwise, we have nothing to compare against, so we shouldn't emit anything.
  bool populated;

  // If true, this directory was present and scannable the last time it was encountered in the polling cycle. Used to
  // prevent duplicate deletion events for missing directories.
  bool was_present;

  // For great logging.
  friend std::ostream &operator<<(std::ostream &out, const DirectoryRecord &record)
  {
    out << "DirectoryRecord{" << record.name << " entries=" << record.entries.size()
        << " subdirectories=" << record.subdirectories.size();
    if (record.populated) out << " populated";
    return out << "}";
  }
};

#endif
