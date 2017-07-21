#ifndef RENAME_BUFFER_H
#define RENAME_BUFFER_H

#include <string>
#include <unordered_map>
#include <utility>
#include <sys/stat.h>

#include "recent_file_cache.h"
#include "../../message.h"

class EventHandler;

// Filesystem entry that was flagged as participating in a rename by a received filesystem event.
class RenameBufferEntry {
public:
  RenameBufferEntry(RenameBufferEntry &&other) :
    path{std::move(other.path)},
    entry_is_present{other.entry_is_present},
    kind{other.kind},
    inode{other.inode},
    size{other.size} {};

  static RenameBufferEntry present(const std::string &path, EntryKind kind, ino_t inode, size_t size);
  static RenameBufferEntry absent(const std::string &path, EntryKind kind, ino_t last_inode, size_t last_size);

  bool is_present();

private:
  RenameBufferEntry(const std::string &path, EntryKind kind, bool present, ino_t inode, size_t size) :
    path{path}, entry_is_present{present}, kind{kind}, inode{inode}, size{size} {};

  std::string path;
  bool entry_is_present;
  EntryKind kind;
  ino_t inode;
  size_t size;

  friend class RenameBuffer;
};

class RenameBuffer {
public:
  // Create a new buffer with a reference to the EventHandler it should use to enqueue messages.
  RenameBuffer(EventHandler *handler) : handler{handler} {};

  // Observe a rename event for an entry that's still present in the filesystem, with the results of a successful
  // lstat() call. May emit a "rename" message.
  void observe_present_entry(const std::string &path, EntryKind kind, ino_t inode, size_t size);

  // Observe a rename event for an entry that is no longer present in the filesystem and has no historic lstat()
  // data in the cache. May emit a "rename" message.
  void observe_absent_entry(const std::string &path, EntryKind kind);

  // Observe a rename event for an entry that is no longer present in the filesystem, but does have lstat() data
  // in the cache.
  void observe_absent_entry(const std::string &path, EntryKind kind, ino_t last_inode, size_t last_size);

  // Enqueue creation and removal events for any buffer entries that have not been paired during the current
  // event handler callback invocation.
  void flush_unmatched();
private:
  EventHandler *handler;

  std::unordered_map<ino_t, RenameBufferEntry> observed_by_inode;
};

#endif
