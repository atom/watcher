#include <memory>
#include <string>
#include <stack>
#include <queue>
#include <set>

#include "directory_record.h"
#include "../message_buffer.h"
#include "polling_iterator.h"

using std::string;
using std::shared_ptr;
using std::set;

PollingIterator::PollingIterator(shared_ptr<DirectoryRecord> root) :
  root(root),
  current(root),
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
      advance_scan();
    } else if (iterator.phase == PollingIterator::ENTRIES) {
      advance_entry();
    } else if (iterator.phase == PollingIterator::RESET) {
      break;
    }
    count++;
  }

  if (iterator.phase == PollingIterator::RESET) {
    iterator.current = iterator.root;
    iterator.phase = PollingIterator::SCAN;
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
    iterator.current->entry(this, *(iterator.current_entry));
    iterator.current_entry++;
  }

  if (iterator.current_entry != iterator.entries.end()) {
    // Remain in ENTRIES phase
    return;
  }

  iterator.entries.clear();
  iterator.current_entry = iterator.entries.end();

  if (iterator.directories.empty()) {
    iterator.phase = PollingIterator::RESET;
    return;
  }

  // Advance to the next directory in the queue
  iterator.current = iterator.directories.front();
  iterator.directories.pop();
  iterator.phase = PollingIterator::SCAN;
}
