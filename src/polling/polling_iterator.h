#ifndef POLLING_ITERATOR
#define POLLING_ITERATOR

#include <iostream>
#include <memory>
#include <queue>
#include <stack>
#include <string>
#include <utility>
#include <uv.h>

#include "../message.h"
#include "../message_buffer.h"

class DirectoryRecord;

// Persistent state of the iteration over the contents of a `PolledRoot`. This allows `PolledRoot` to partially scan
// large filesystems, then resume after a pause.
//
// `BoundPollingIterator` does most of the actual work, but stores all of its persistent state here.
class PollingIterator
{
public:
  // Create an iterator poised to begin at a root `DirectoryRecord`. If `recursive` is true, the iterator will
  // automatically advance into subdirectories of the root.
  explicit PollingIterator(const std::shared_ptr<DirectoryRecord> &root, bool recursive);

  PollingIterator(const PollingIterator &) = delete;
  PollingIterator(PollingIterator &&) = delete;
  ~PollingIterator() = default;
  PollingIterator &operator=(const PollingIterator &) = delete;
  PollingIterator &operator=(PollingIterator &&) = delete;

private:
  // The top-level `DirectoryRecord` of the `PolledRoot`, so we know where to reset when we reach the end.
  std::shared_ptr<DirectoryRecord> root;

  // If `true`, the iterator will automatically descend into subdirectories as they are discovered.
  bool recursive;

  // The `DirectoryRecord` that we're on right now.
  std::shared_ptr<DirectoryRecord> current;

  // Remember the current `DirectoryRecord`'s full, joined path to avoid recursing up the entire tree for each entry.
  std::string current_path;

  // An entry name and `EntryKind` pair reported by a `scandir()` call. Populated by
  // `BoundPollingIterator::advance_scan()` in the `SCAN` phase.
  std::vector<Entry> entries;

  // Save our place within the `entries` vector during the `ENTRIES` phase.
  std::vector<Entry>::iterator current_entry;

  // A queue of subdirectories to traverse next. Populated by `BoundPollingIterator::advance_scan()` in the `SCAN`
  // phase.
  std::queue<std::shared_ptr<DirectoryRecord>> directories;

  // Phases of traversal.
  enum
  {
    SCAN,  // Scan the current `DirectoryRecord` to populate `entries`,
    ENTRIES,  // Compare the next entry to an up-to-date `lstat()` result to see if an entry has changed.
    RESET  // Loop back to the root directory.
  } phase;

  friend class BoundPollingIterator;

  // Always handy to have.
  friend std::ostream &operator<<(std::ostream &out, const PollingIterator &iterator)
  {
    out << "PollingIterator{at ";
    out << iterator.current_path;
    out << " phase=";
    switch (iterator.phase) {
      case SCAN: out << "SCAN"; break;
      case ENTRIES: out << "ENTRIES"; break;
      case RESET: out << "RESET"; break;
      default: out << "!!phase=" << iterator.phase; break;
    }
    out << " entries=" << iterator.entries.size();
    out << " directories=" << iterator.directories.size();
    out << "}";
    return out;
  }
};

// Bind a `PollingIterator` to a `ChannelMessageBuffer` that should be used to emit any discovered events.
//
// `PollingIterator` stores all of the persistent state, but `BoundPollingIterator` performs most of the work.
class BoundPollingIterator
{
public:
  // Bind an existing `PollingIterator` containing persistent polling state with a `ChannelMessageBuffer` that
  // determines where events emitted by this polling cycle should be sent.
  BoundPollingIterator(PollingIterator &iterator, ChannelMessageBuffer &buffer);

  BoundPollingIterator(const BoundPollingIterator &) = delete;
  BoundPollingIterator(BoundPollingIterator &&) = delete;
  ~BoundPollingIterator() = default;
  BoundPollingIterator &operator=(const BoundPollingIterator &) = delete;
  BoundPollingIterator &operator=(BoundPollingIterator &&) = delete;

  // Called from `DirectoryRecord::scan()` to make note of an entry within the current directory.
  void push_entry(std::string &&entry, EntryKind kind) { iterator.entries.emplace_back(std::move(entry), kind); }

  // Called from `DirectoryRecord::entry()` when a subdirectory is encountered to enqueue it for traversal.
  void push_directory(const std::shared_ptr<DirectoryRecord> &subdirectory)
  {
    if (iterator.recursive) iterator.directories.push(subdirectory);
  }

  // Access the message buffer to emit events from other classes.
  ChannelMessageBuffer &get_buffer() { return buffer; }

  // Allow the `DirectoryRecord` to determine whether or not this iteration is recursive.
  bool is_recursive() { return iterator.recursive; }

  // Perform at most `throttle_allocation` filesystem operations, emitting events and updating records appropriately. If
  // the end of the filesystem tree is reached, the iteration will stop and leave the `PollingIterator` ready to resume
  // at the root on the next call.
  //
  // Return the number of operations actually performed.
  size_t advance(size_t throttle_allocation);

private:
  // Scan the current directory with `DirectoryRecord::scan()`, populating our iterator's `entries` map. Leave the
  // iterator ready to advance through the discovered entries.
  void advance_scan();

  // Perform a single stat call with `DirectoryRecord::entry()`. Advance the `current_entry`. If no more entries
  // remain, pop the next `DirectoryRecord` from the queue. If the queue is empty, reset the iterator back to its
  // root.
  void advance_entry();

  ChannelMessageBuffer &buffer;
  PollingIterator &iterator;

  friend std::ostream &operator<<(std::ostream &out, const BoundPollingIterator &it)
  {
    return out << "Bound{channel=" << it.buffer.get_channel_id() << " " << it.iterator << "}";
  }
};

#endif
