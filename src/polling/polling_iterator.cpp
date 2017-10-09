#include <memory>
#include <queue>
#include <set>
#include <stack>
#include <string>

#include "../helper/common.h"
#include "../log.h"
#include "../message_buffer.h"
#include "directory_record.h"
#include "polling_iterator.h"

using std::endl;
using std::set;
using std::shared_ptr;
using std::string;

PollingIterator::PollingIterator(shared_ptr<DirectoryRecord> root) :
  root(root),
  current(root),
  current_path(root->path()),
  phase{PollingIterator::SCAN}
{
  //
}

PollingIterator::~PollingIterator()
{
  //
}

BoundPollingIterator::BoundPollingIterator(PollingIterator &iterator, ChannelMessageBuffer &buffer) :
  buffer{buffer},
  iterator{iterator}
{
  //
}

BoundPollingIterator::~BoundPollingIterator()
{
  //
}

size_t BoundPollingIterator::advance(size_t throttle_allocation)
{
  size_t total = throttle_allocation > 0 ? throttle_allocation : 1;
  size_t count = 0;

  while (count < total) {
    if (iterator.phase == PollingIterator::SCAN) {
      LOGGER << *this << " performing SCAN." << endl;
      advance_scan();
    } else if (iterator.phase == PollingIterator::ENTRIES) {
      LOGGER << *this << " performing ENTRIES." << endl;
      advance_entry();
    } else if (iterator.phase == PollingIterator::RESET) {
      LOGGER << *this << " detected a RESET." << endl;
      break;
    }
    count++;
  }

  if (iterator.phase == PollingIterator::RESET) {
    iterator.current = iterator.root;
    iterator.current_path = iterator.current->path();
    iterator.phase = PollingIterator::SCAN;

    LOGGER << *this << " has been reset back to its root." << endl;
  }

  return count;
}

void BoundPollingIterator::advance_scan()
{
  iterator.current->scan(this);

  iterator.current_entry = iterator.entries.begin();
  iterator.phase = PollingIterator::ENTRIES;
}

void BoundPollingIterator::advance_entry()
{
  if (iterator.current_entry != iterator.entries.end()) {
    string &entry_name = iterator.current_entry->first;
    EntryKind kind = iterator.current_entry->second;

    iterator.current->entry(this, entry_name, path_join(iterator.current_path, entry_name), kind);
    iterator.current_entry++;
  }

  if (iterator.current_entry != iterator.entries.end()) {
    // Remain in ENTRIES phase
    return;
  }

  iterator.current->mark_populated();
  iterator.entries.clear();
  iterator.current_entry = iterator.entries.end();

  if (iterator.directories.empty()) {
    iterator.phase = PollingIterator::RESET;
    return;
  }

  // Advance to the next directory in the queue
  iterator.current = iterator.directories.front();
  iterator.current_path = iterator.current->path();
  iterator.directories.pop();
  iterator.phase = PollingIterator::SCAN;
}
