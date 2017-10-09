#ifndef SIDE_EFFECT_H
#define SIDE_EFFECT_H

#include <string>
#include <utility>
#include <vector>

#include "../../message.h"
#include "../../result.h"

// Forward declaration for pointer access.
class WatchRegistry;

// Record additional actions that should be triggered by inotify events received in the course of a single notification
// cycle.
class SideEffect
{
public:
  SideEffect() = default;

  // Recursively watch a newly created subdirectory.
  void track_subdirectory(std::string subdir, ChannelID channel_id);

  // Perform all enqueued actions.
  Result<> enact_in(WatchRegistry *registry);

private:
  SideEffect(const SideEffect &other) = delete;
  SideEffect(SideEffect &&other) = delete;
  SideEffect &operator=(const SideEffect &other) = delete;
  SideEffect &operator=(SideEffect &&other) = delete;

  struct Subdirectory
  {
    Subdirectory(std::string &&path, ChannelID channel_id) : path(std::move(path)), channel_id{channel_id}
    {
      //
    }

    std::string path;
    ChannelID channel_id;
  };

  std::vector<Subdirectory> subdirectories;
};

#endif
