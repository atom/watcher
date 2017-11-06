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
#include "recent_file_cache.h"

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
  current{original.current},
  age{original.age}
{}

RenameBufferEntry::RenameBufferEntry(std::shared_ptr<PresentEntry> entry, bool current) :
  entry{std::move(entry)},
  current{current},
  age{0}
{}

bool RenameBuffer::observe_event(Event &event, RecentFileCache &cache)
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
    if (observe_present_entry(event.message_buffer(), cache, current_present, true)) handled = true;
  }

  if (former->is_present()) {
    shared_ptr<PresentEntry> former_present = static_pointer_cast<PresentEntry>(former);
    if (observe_present_entry(event.message_buffer(), cache, former_present, false)) handled = true;
  }

  if (former->is_absent() && current->is_absent()) {
    shared_ptr<AbsentEntry> current_absent = static_pointer_cast<AbsentEntry>(current);
    if (observe_absent(event.message_buffer(), cache, current_absent)) handled = true;
  }

  return handled;
}

bool RenameBuffer::observe_present_entry(ChannelMessageBuffer &message_buffer,
  RecentFileCache &cache,
  const shared_ptr<PresentEntry> &present,
  bool current)
{
  ostream &logline = LOGGER << "Rename ";

  auto maybe_entry = observed_by_inode.find(present->get_inode());
  if (maybe_entry == observed_by_inode.end()) {
    // The first-seen half of this rename event. Buffer a new entry to be paired with the second half when or if it's
    // observed.
    RenameBufferEntry entry(present, current);
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

      cache.evict(existing.entry);
      message_buffer.renamed(
        string(existing.entry->get_path()), string(present->get_path()), present->get_entry_kind());
      handled = true;
    } else if (existing.current && !current) {
      // The former end is the "to" end and the current end is the "from" end.
      logline << "completed pair " << *present << " => " << *existing.entry << ": Emitting rename event." << endl;

      cache.evict(present);
      message_buffer.renamed(
        string(present->get_path()), string(existing.entry->get_path()), existing.entry->get_entry_kind());
      handled = true;
    } else {
      // Either both entries are still present (hardlink, re-used inode?) or both are missing (rapidly renamed and
      // deleted?) This could happen if the entry is renamed again between the lstat() calls, possibly.
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

bool RenameBuffer::observe_absent(ChannelMessageBuffer &message_buffer,
  RecentFileCache & /*cache*/,
  const std::shared_ptr<AbsentEntry> &absent)
{
  LOGGER << "Unable to correlate rename from " << absent->get_path() << " without an inode." << endl;
  message_buffer.created(string(absent->get_path()), absent->get_entry_kind());
  message_buffer.deleted(string(absent->get_path()), absent->get_entry_kind());
  return true;
}

shared_ptr<set<RenameBuffer::Key>> RenameBuffer::flush_unmatched(ChannelMessageBuffer &message_buffer)
{
  shared_ptr<set<Key>> all(new set<Key>);

  for (auto &it : observed_by_inode) {
    all->insert(it.first);
  }

  return flush_unmatched(message_buffer, all);
}

shared_ptr<set<RenameBuffer::Key>> RenameBuffer::flush_unmatched(ChannelMessageBuffer &message_buffer,
  const shared_ptr<set<Key>> &keys)
{
  shared_ptr<set<Key>> aged(new set<Key>);
  vector<Key> to_erase;

  for (auto &it : observed_by_inode) {
    const Key &key = it.first;
    if (keys->count(key) == 0) continue;

    RenameBufferEntry &existing = it.second;
    shared_ptr<PresentEntry> entry = existing.entry;

    if (existing.age == 0u) {
      existing.age++;
      aged->insert(key);
      continue;
    }

    if (existing.current) {
      message_buffer.created(string(entry->get_path()), entry->get_entry_kind());
    } else {
      message_buffer.deleted(string(entry->get_path()), entry->get_entry_kind());
    }
    to_erase.push_back(key);
  }

  for (Key &key : to_erase) {
    observed_by_inode.erase(key);
  }

  return aged;
}
