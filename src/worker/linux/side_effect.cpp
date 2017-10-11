#include <string>
#include <utility>
#include <vector>

#include "../../message.h"
#include "../../result.h"
#include "side_effect.h"
#include "watch_registry.h"

using std::move;
using std::string;
using std::vector;

void SideEffect::track_subdirectory(string subdir, ChannelID channel_id)
{
  subdirectories.emplace_back(move(subdir), channel_id);
}

Result<> SideEffect::enact_in(WatchRegistry *registry, vector<PollingRoot> &poll)
{
  Result<> r = ok_result();
  for (Subdirectory &subdir : subdirectories) {
    vector<string> poll_roots;
    r &= registry->add(subdir.channel_id, subdir.path, true, poll_roots);

    for (string &poll_root : poll_roots) {
      poll.emplace_back(subdir.channel_id, move(poll_root));
    }
  }
  return r;
}
