#ifndef SIDE_EFFECT_H
#define SIDE_EFFECT_H

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../../message.h"
#include "../../result.h"

// Forward declaration for pointer access.
class WatchRegistry;

class MessageBuffer;

class WatchedDirectory;

// Record additional actions that should be triggered by inotify events received in the course of a single notification
// cycle.
class SideEffect
{
public:
  SideEffect() = default;
  ~SideEffect() = default;

  // Recursively watch a newly created subdirectory.
  void track_subdirectory(std::string subdir, ChannelID channel_id);

  // Unsubscribe from a channel after this event has been handled.
  void remove_channel(ChannelID channel_id) { removed_roots.insert(channel_id); }

  // Perform all enqueued actions.
  void enact_in(const std::shared_ptr<WatchedDirectory> &parent, WatchRegistry *registry, MessageBuffer &messages);

  SideEffect(const SideEffect &other) = delete;
  SideEffect(SideEffect &&other) = delete;
  SideEffect &operator=(const SideEffect &other) = delete;
  SideEffect &operator=(SideEffect &&other) = delete;

private:
  struct Subdirectory
  {
    Subdirectory(std::string &&basename, ChannelID channel_id) : basename(std::move(basename)), channel_id{channel_id}
    {
      //
    }

    Subdirectory(Subdirectory &&original) : basename{std::move(original.basename)}, channel_id{original.channel_id}
    {
      //
    }

    ~Subdirectory() = default;

    std::string basename;
    ChannelID channel_id;

    Subdirectory(const Subdirectory &) = delete;
    Subdirectory &operator=(const Subdirectory &) = delete;
    Subdirectory &operator=(Subdirectory &&) = delete;
  };

  std::vector<Subdirectory> subdirectories;

  std::set<ChannelID> removed_roots;
};

#endif
