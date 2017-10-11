#include <string>
#include <utility>
#include <vector>

#include "../../message.h"
#include "../../result.h"
#include "side_effect.h"
#include "watch_registry.h"

using std::move;
using std::string;

void SideEffect::track_subdirectory(string subdir, ChannelID channel_id)
{
  subdirectories.emplace_back(move(subdir), channel_id);
}

Result<> SideEffect::enact_in(WatchRegistry *registry)
{
  Result<> r = ok_result();
  for (Subdirectory &subdir : subdirectories) {
    r &= registry->add(subdir.channel_id, subdir.path, true);
  }
  return r;
}
