#ifndef RENAME_BUFFER_H
#define RENAME_BUFFER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <utility>
#include <sys/stat.h>

#include "recent_file_cache.h"
#include "../../message.h"

class EventHandler;

// Filesystem entry that was flagged as participating in a rename by a received filesystem event.
class RenameBufferEntry {
public:
  RenameBufferEntry(RenameBufferEntry &&original) :
    entry{std::move(original.entry)},
    current{original.current} {};

private:
  RenameBufferEntry(std::shared_ptr<PresentEntry> entry, bool current) :
    entry{entry}, current{current} {};

  std::shared_ptr<PresentEntry> entry;
  bool current;

  friend class RenameBuffer;
};

class RenameBuffer {
public:
  // Create a new buffer with a reference to the EventHandler it should use to enqueue messages.
  RenameBuffer(EventHandler *handler) : handler{handler} {};

  // Observe a rename event for a filesystem event. Deduce the matching side of the rename, if possible,
  // based on the previous and currently observed state of the entry at that path.
  void observe_entry(std::shared_ptr<StatResult> former, std::shared_ptr<StatResult> current);

  // Enqueue creation and removal events for any buffer entries that have not been paired during the current
  // event handler callback invocation.
  void flush_unmatched();
private:
  void observe_present_entry(std::shared_ptr<PresentEntry> present, bool current);

  EventHandler *handler;

  std::unordered_map<ino_t, RenameBufferEntry> observed_by_inode;
};

#endif
