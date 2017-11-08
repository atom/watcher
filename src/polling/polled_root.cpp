#include <string>
#include <utility>

#include "../message.h"
#include "../message_buffer.h"
#include "directory_record.h"
#include "polled_root.h"

using std::move;
using std::string;

PolledRoot::PolledRoot(string &&root_path, ChannelID channel_id, bool recursive) :
  root(new DirectoryRecord(move(root_path))),
  channel_id{channel_id},
  iterator(root, recursive),
  all_populated{false}
{
  //
}

size_t PolledRoot::advance(MessageBuffer &buffer, size_t throttle_allocation)
{
  ChannelMessageBuffer channel_buffer(buffer, channel_id);
  BoundPollingIterator bound_iterator(iterator, channel_buffer);

  size_t progress = bound_iterator.advance(throttle_allocation);

  if (!all_populated && root->all_populated()) {
    all_populated = true;
  }

  return progress;
}

size_t PolledRoot::count_entries() const
{
  return root->count_entries();
}
