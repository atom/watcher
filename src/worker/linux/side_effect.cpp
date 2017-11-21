#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../message.h"
#include "../../message_buffer.h"
#include "../../result.h"
#include "side_effect.h"
#include "watch_registry.h"

using std::move;
using std::shared_ptr;
using std::string;
using std::vector;

void SideEffect::track_subdirectory(string subdir, ChannelID channel_id)
{
  subdirectories.emplace_back(move(subdir), channel_id);
}

void SideEffect::enact_in(const shared_ptr<WatchedDirectory> &parent, WatchRegistry *registry, MessageBuffer &messages)
{
  for (ChannelID channel_id : removed_roots) {
    Result<> r = registry->remove(channel_id);
    if (r.is_error()) messages.error(channel_id, string(r.get_error()), false);
  }

  for (Subdirectory &subdir : subdirectories) {
    if (removed_roots.find(subdir.channel_id) != removed_roots.end()) {
      continue;
    }

    vector<string> poll_roots;
    Result<> r = registry->add(subdir.channel_id, parent, subdir.basename, true, poll_roots);
    if (r.is_error()) messages.error(subdir.channel_id, string(r.get_error()), false);

    for (string &poll_root : poll_roots) {
      messages.add(Message(CommandPayloadBuilder::add(subdir.channel_id, move(poll_root), true, 1).build()));
    }
  }
}
