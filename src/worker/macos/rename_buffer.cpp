#include "rename_buffer.h"

#include <string>
#include <memory>
#include <utility>
#include <sys/stat.h>

#include "event_handler.h"
#include "recent_file_cache.h"
#include "../../log.h"

using std::string;
using std::move;
using std::endl;
using std::shared_ptr;
using std::static_pointer_cast;

void RenameBuffer::observe_entry(shared_ptr<StatResult> former, shared_ptr<StatResult> current)
{
  if (!former->has_changed_from(*current)) {
    // The entry is still there with the same inode.
    // Moved away and back, maybe?
    return;
  }

  if (former->is_present()) {
    shared_ptr<PresentEntry> former_present = static_pointer_cast<PresentEntry>(former);
    observe_present_entry(former_present, false);
  }

  if (current->is_present()) {
    shared_ptr<PresentEntry> current_present = static_pointer_cast<PresentEntry>(current);
    observe_present_entry(current_present, true);
  }
}

void RenameBuffer::observe_present_entry(shared_ptr<PresentEntry> present, bool current)
{
  auto maybe_entry = observed_by_inode.find(present->get_inode());
  if (maybe_entry == observed_by_inode.end()) {
    // The first-seen half of this rename event. Buffer a new entry to be paired with the second half when or if it's
    // observed.
    RenameBufferEntry entry(present, current);
    observed_by_inode.emplace(present->get_inode(), move(entry));
    LOGGER << "First half of rename (present) observed: " << *present << "." << endl;
    return;
  }
  RenameBufferEntry &existing = maybe_entry->second;

  LOGGER << "Found existing entry with same inode: " << *(existing.entry) << endl;

  if (present->could_be_rename_of(*(existing.entry))) {
    // The inodes and entry kinds match, so with high probability, we've found the other half of the rename.
    // Huzzah! Huzzah forever!
    LOGGER << "Second half of rename (present) observed: " << *present << "." << endl;

    // The former end is the "from" end and the current end is the "to" end.
    if (!existing.current && current) {
      handler->enqueue_rename(existing.entry->get_path(), present->get_path(), present->get_entry_kind());
    } else if (existing.current && !current) {
      handler->enqueue_rename(present->get_path(), existing.entry->get_path(), existing.entry->get_entry_kind());
    } else {
      // Either both entries are still present (re-used inode?) or both are missing (rapidly renamed and deleted?)
      // This could happen if the entry is renamed again between the lstat() calls, possibly.
      string existing_desc = existing.current ? " (current) " : " (former) ";
      string incoming_desc = current ? " (current) " : " (former) ";

      LOGGER
        << "Current entry: "
        << *present << incoming_desc
        << " conflicts with buffered entry: "
        << *(existing.entry) << existing_desc
        << ". Unable to correlate rename event."
        << endl;
    }

    observed_by_inode.erase(maybe_entry);
  } else {
    LOGGER << "Rename entry " << *present << " conflicts with existing entry " << *(existing.entry) << "." << endl;
  }
}

void RenameBuffer::flush_unmatched()
{
  for (auto it = observed_by_inode.begin(); it != observed_by_inode.end(); ++it) {
    RenameBufferEntry &existing = it->second;
    shared_ptr<PresentEntry> entry = existing.entry;

    if (existing.current) {
      handler->enqueue_creation(entry->get_path(), entry->get_entry_kind());
    } else {
      handler->enqueue_deletion(entry->get_path(), entry->get_entry_kind());
    }
  }
}
