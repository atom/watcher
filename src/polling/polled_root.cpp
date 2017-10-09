#include <string>
#include <utility>

#include "../message.h"
#include "../message_buffer.h"
#include "directory_record.h"
#include "polled_root.h"

using std::move;
using std::string;

PolledRoot::PolledRoot(string &&root_path, CommandID command_id, ChannelID channel_id) :
  root(new DirectoryRecord(move(root_path))),
  command_id{command_id},
  channel_id{channel_id},
  iterator(root)
{
  //
}

size_t PolledRoot::advance(MessageBuffer &buffer, size_t throttle_allocation)
{
  ChannelMessageBuffer channel_buffer(buffer, channel_id);
  BoundPollingIterator bound_iterator(iterator, channel_buffer);

  size_t progress = bound_iterator.advance(throttle_allocation);

  if (command_id != NULL_COMMAND_ID && root->all_populated()) {
    channel_buffer.ack(command_id, true, "");
    command_id = NULL_COMMAND_ID;
  }

  return progress;
}
