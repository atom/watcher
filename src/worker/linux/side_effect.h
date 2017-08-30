#ifndef SIDE_EFFECT_H
#define SIDE_EFFECT_H

#include <string>
#include <vector>
#include <utility>

#include "../../message.h"
#include "../../result.h"

class WatchRegistry;

class SideEffect {
public:
  void track_subdirectory(std::string subdir, ChannelID channel_id);

  Result<> enact_in(WatchRegistry *registry);

private:
  SideEffect(const SideEffect &other) = delete;
  SideEffect(SideEffect &&other) = delete;
  SideEffect &operator=(const SideEffect &other) = delete;
  SideEffect &operator=(SideEffect &&other) = delete;

  struct Subdirectory {
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
