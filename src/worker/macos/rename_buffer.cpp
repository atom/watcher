#include "rename_buffer.h"

#include <string>
#include <utility>
#include <sys/stat.h>

#include "event_handler.h"

using std::string;
using std::move;

RenameBufferEntry RenameBufferEntry::present(const string &path, EntryKind kind, ino_t inode, size_t size)
{
  return RenameBufferEntry(path, kind, true, inode, size);
}

RenameBufferEntry RenameBufferEntry::absent(const string &path, EntryKind kind, ino_t inode, size_t size)
{
  return RenameBufferEntry(path, kind, false, inode, size);
}

bool RenameBufferEntry::is_present()
{
  return entry_is_present;
}

void RenameBuffer::observe_present_entry(const std::string &path, EntryKind kind, ino_t inode, size_t size)
{
  auto maybe_entry = observed_by_inode.find(inode);
  if (maybe_entry == observed_by_inode.end()) {
    // The first-seen half of this rename event. Observe a new entry to be paired with the second half when it's
    // observed.
    RenameBufferEntry entry = RenameBufferEntry::present(path, kind, inode, size);
    observed_by_inode.emplace(inode, move(entry));
    return;
  }
  RenameBufferEntry &existing = maybe_entry->second;

  if (existing.kind == kind && existing.size == size && !existing.is_present()) {
    // The inodes, size, and entry kinds match, so with high probability, we've found the other half of the rename.
    // Huzzah! Huzzah forever!

    // The absent end is the "from" end and the present end is the "to" end.
    handler->enqueue_rename(existing.path, path, kind);
    observed_by_inode.erase(maybe_entry);
  }
}

void RenameBuffer::observe_absent_entry(const std::string &path, EntryKind kind)
{
  // If we don't have a cached inode to correllate against, just interpret this as a deletion.
  handler->enqueue_deletion(path, kind);
}

void RenameBuffer::observe_absent_entry(const std::string &path, EntryKind kind, ino_t last_inode, size_t last_size)
{
  auto maybe_entry = observed_by_inode.find(last_inode);
  if (maybe_entry == observed_by_inode.end()) {
    // The first-seen half of this rename event. Observe a new entry to be paired with the second half when it's
    // observed.
    RenameBufferEntry entry = RenameBufferEntry::absent(path, kind, last_inode, last_size);
    observed_by_inode.emplace(last_inode, move(entry));
    return;
  }
  RenameBufferEntry &existing = maybe_entry->second;

  if (existing.kind == kind && existing.size == last_size && existing.is_present()) {
    // The inodes, size, and entry kinds match, so with high probability, we've found the other half of the rename.
    handler->enqueue_rename(path, existing.path, kind);
    observed_by_inode.erase(maybe_entry);
  }
}

void RenameBuffer::flush_unmatched()
{
  for (auto it = observed_by_inode.begin(); it != observed_by_inode.end(); ++it) {
    RenameBufferEntry &existing = it->second;
    if (existing.is_present()) {
      handler->enqueue_creation(existing.path, existing.kind);
    } else {
      handler->enqueue_deletion(existing.path, existing.kind);
    }
  }
}
