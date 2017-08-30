#include <string>
#include <vector>
#include <utility>

#include "../../message.h"
#include "../../result.h"
#include "watch_registry.h"
#include "side_effect.h"

using std::string;
using std::move;

void SideEffect::track_subdirectory(string subdir, ChannelID channel_id)
{
  subdirectories.emplace_back(move(subdir), channel_id);
}

Result<> SideEffect::enact_in(WatchRegistry *registry)
{
  Result<> r = ok_result();
  for (Subdirectory &subdir : subdirectories) {
    r.accumulate(registry->add(subdir.channel_id, subdir.path, true));
  }
  return r;
}
