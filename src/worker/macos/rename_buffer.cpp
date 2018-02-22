#include "rename_buffer.h"

#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "../../log.h"
#include "batch_handler.h"

using std::endl;
using std::move;
using std::ostream;
using std::set;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;

RenameBufferEntry::RenameBufferEntry(RenameBufferEntry &&original) noexcept :
  entry(move(original.entry)),
  event_path(move(original.event_path)),
  current{original.current},
  age{original.age}
{
  //
}

RenameBufferEntry::RenameBufferEntry(shared_ptr<PresentEntry> entry, string event_path, bool current) :
  entry{move(entry)},
  event_path{move(event_path)},
  current{current},
  age{0}
{
  //
}

bool RenameBuffer::observe_event(Event &event, BatchHandler &batch)
{
  const shared_ptr<StatResult> &former = event.get_former();
  const shared_ptr<StatResult> &current = event.get_current();
  bool handled = false;

  if (current->could_be_rename_of(*former)) {
    // inode and entry kind are still the same. Stale ItemRenamed bit, most likely.
    return false;
  }

  if (current->is_present()) {
    shared_ptr<PresentEntry> current_present = static_pointer_cast<PresentEntry>(current);
    if (observe_present_entry(event, batch, current_present, true)) handled = true;
  }

  if (former->is_present()) {
    shared_ptr<PresentEntry> former_present = static_pointer_cast<PresentEntry>(former);
    if (observe_present_entry(event, batch, former_present, false)) handled = true;
  }

  if (former->is_absent() && current->is_absent()) {
    shared_ptr<AbsentEntry> current_absent = static_pointer_cast<AbsentEntry>(current);
    if (observe_absent(event, batch, current_absent)) handled = true;
  }

  return handled;
}

bool RenameBuffer::observe_present_entry(Event &event,
  BatchHandler &batch,
  const shared_ptr<PresentEntry> &present,
  bool current)
{
  ostream &logline = LOGGER << "Rename ";

  auto maybe_entry = observed_by_inode.find(present->get_inode());
  if (maybe_entry == observed_by_inode.end()) {
    // The first-seen half of this rename event. Buffer a new entry to be paired with the second half when or if it's
    // observed.
    RenameBufferEntry entry(present, event.get_event_path(), current);
    observed_by_inode.emplace(present->get_inode(), move(entry));
    logline << "first half " << *present << ": Remembering for later." << endl;
    return true;
  }
  RenameBufferEntry &existing = maybe_entry->second;

  if (present->could_be_rename_of(*(existing.entry))) {
    // The inodes and entry kinds match, so with high probability, we've found the other half of the rename.
    // Huzzah! Huzzah forever!
    bool handled = false;

    if (!existing.current && current) {
      // The former end is the "from" end and the current end is the "to" end.
      logline << "completed pair " << *existing.entry << " => " << *present << ": Emitting rename event." << endl;

      EntryKind kind = present->get_entry_kind();
      string from_path(existing.event_path);
      string to_path(event.get_event_path());

      event.cache().evict(existing.entry);
      if (kind == KIND_DIRECTORY || kind == KIND_UNKNOWN) {
        batch.update_for_rename(from_path, to_path);
      }
      event.message_buffer().renamed(move(from_path), move(to_path), kind);
      handled = true;
    } else if (existing.current && !current) {
      // The former end is the "to" end and the current end is the "from" end.
      logline << "completed pair " << *present << " => " << *existing.entry << ": Emitting rename event." << endl;

      EntryKind kind = existing.entry->get_entry_kind();
      string from_path(event.get_event_path());
      string to_path(existing.event_path);

      event.cache().evict(present);
      if (kind == KIND_DIRECTORY || kind == KIND_UNKNOWN) {
        batch.update_for_rename(from_path, to_path);
      }
      event.message_buffer().renamed(move(from_path), move(to_path), kind);
      handled = true;
    } else {
      // Both entries are still present (hardlink, re-used inode?).
      string existing_desc = existing.current ? " (current) " : " (former) ";
      string incoming_desc = current ? " (current) " : " (former) ";

      logline << "conflicting pair " << *present << incoming_desc << " =/= " << *(existing.entry) << existing_desc
              << "are both present." << endl;
      handled = false;
    }

    observed_by_inode.erase(maybe_entry);
    return handled;
  }

  string existing_desc = existing.current ? " (current) " : " (former) ";
  string incoming_desc = current ? " (current) " : " (former) ";

  logline << "conflicting pair " << *present << incoming_desc << " =/= " << *(existing.entry) << existing_desc
          << "have conflicting entry kinds." << endl;
  return false;
}

bool RenameBuffer::observe_absent(Event &event, BatchHandler & /*batch*/, const std::shared_ptr<AbsentEntry> &absent)
{
  const string &event_path = event.get_event_path();

  LOGGER << "Unable to correlate rename from " << event_path << " without an inode." << endl;
  if (event.flag_created() ^ event.flag_deleted()) {
    // this entry was created just before being renamed or deleted just after being renamed.
    event.message_buffer().created(string(event_path), absent->get_entry_kind());
    event.message_buffer().deleted(string(event_path), absent->get_entry_kind());
  } else if (!event.flag_created() && !event.flag_deleted()) {
    // former must have been evicted from the cache.
    event.message_buffer().deleted(string(event_path), absent->get_entry_kind());
  }
  return true;
}

shared_ptr<set<RenameBuffer::Key>> RenameBuffer::flush_unmatched(ChannelMessageBuffer &message_buffer,
  RecentFileCache &cache)
{
  shared_ptr<set<Key>> all(new set<Key>);

  for (auto &it : observed_by_inode) {
    all->insert(it.first);
  }

  return flush_unmatched(message_buffer, cache, all);
}

shared_ptr<set<RenameBuffer::Key>> RenameBuffer::flush_unmatched(ChannelMessageBuffer &message_buffer,
  RecentFileCache &cache,
  const shared_ptr<set<Key>> &keys)
{
  shared_ptr<set<Key>> aged(new set<Key>);
  vector<Key> to_erase;

  for (auto &it : observed_by_inode) {
    const Key &key = it.first;
    if (keys->count(key) == 0) continue;

    RenameBufferEntry &existing = it.second;
    const string &event_path = existing.event_path;
    shared_ptr<PresentEntry> entry = existing.entry;

    if (existing.age == 0u) {
      existing.age++;
      aged->insert(key);
      continue;
    }

    if (existing.current) {
      message_buffer.created(string(event_path), entry->get_entry_kind());
    } else {
      message_buffer.deleted(string(event_path), entry->get_entry_kind());
      cache.evict(event_path);
    }
    to_erase.push_back(key);
  }

  for (Key &key : to_erase) {
    observed_by_inode.erase(key);
  }

  return aged;
}
