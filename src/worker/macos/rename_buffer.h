#ifndef RENAME_BUFFER_H
#define RENAME_BUFFER_H

#include <memory>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <utility>

#include "../../message.h"
#include "../../message_buffer.h"
#include "recent_file_cache.h"

// Filesystem entry that was flagged as participating in a rename by a received filesystem event.
class RenameBufferEntry
{
public:
  RenameBufferEntry(RenameBufferEntry &&original) noexcept :
    entry{std::move(original.entry)},
    current{original.current} {};

  ~RenameBufferEntry() = default;

  RenameBufferEntry(const RenameBufferEntry &) = delete;
  RenameBufferEntry &operator=(const RenameBufferEntry &) = delete;
  RenameBufferEntry &operator=(RenameBufferEntry &&) = delete;

private:
  RenameBufferEntry(std::shared_ptr<PresentEntry> entry, bool current) : entry{std::move(entry)}, current{current} {};

  std::shared_ptr<PresentEntry> entry;
  bool current;

  friend class RenameBuffer;
};

class RenameBuffer
{
public:
  // Create a new buffer with a reference to the ChannelMessageBuffer it should use to enqueue messages.
  RenameBuffer(ChannelMessageBuffer &message_buffer) : message_buffer{message_buffer} {};
  ~RenameBuffer() = default;

  // Observe a rename event for a filesystem event. Deduce the matching side of the rename, if possible,
  // based on the previous and currently observed state of the entry at that path.
  void observe_entry(const std::shared_ptr<StatResult> &former, const std::shared_ptr<StatResult> &current);

  // Enqueue creation and removal events for any buffer entries that have not been paired during the current
  // event handler callback invocation.
  void flush_unmatched();

  RenameBuffer(const RenameBuffer &) = delete;
  RenameBuffer(RenameBuffer &&) = delete;
  RenameBuffer &operator=(const RenameBuffer &) = delete;
  RenameBuffer &operator=(RenameBuffer &&) = delete;

private:
  void observe_present_entry(const std::shared_ptr<PresentEntry> &present, bool current);

  ChannelMessageBuffer &message_buffer;

  std::unordered_map<ino_t, RenameBufferEntry> observed_by_inode;
};

#endif
