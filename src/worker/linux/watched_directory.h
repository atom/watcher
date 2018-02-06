#ifndef WATCHED_DIRECTORY
#define WATCHED_DIRECTORY

#include <memory>
#include <sstream>
#include <string>
#include <sys/inotify.h>
#include <vector>

#include "../../message_buffer.h"
#include "../../result.h"
#include "../recent_file_cache.h"
#include "cookie_jar.h"
#include "side_effect.h"

// Associate resources used to watch inotify events that are delivered with a single watch descriptor.
class WatchedDirectory
{
public:
  WatchedDirectory(int wd,
    ChannelID channel_id,
    std::shared_ptr<WatchedDirectory> parent,
    std::string &&name,
    bool recursive);

  ~WatchedDirectory() = default;

  // Interpret a single inotify event. Buffer messages, store or resolve rename Cookies from the CookieJar, and
  // enqueue SideEffects based on the event's mask.
  Result<> accept_event(MessageBuffer &buffer,
    CookieJar &jar,
    SideEffect &side,
    RecentFileCache &cache,
    const inotify_event &event);

  // A parent WatchedDirectory reported that this directory was renamed. Update our internal state immediately so
  // that events on child paths will be reported with the correct path.
  void was_renamed(const std::shared_ptr<WatchedDirectory> &new_parent, const std::string &new_name)
  {
    parent = new_parent;
    name = new_name;
  }

  // Access the Channel ID this WatchedDirectory will broadcast on.
  ChannelID get_channel_id() { return channel_id; }

  // Access the watch descriptor that corresponds to this directory.
  int get_descriptor() { return wd; }

  // Return true if this directory is the root of a recursively watched subtree.
  bool is_root() { return parent == nullptr; }

  // Return the full absolute path to this directory.
  std::string get_absolute_path();

  WatchedDirectory(const WatchedDirectory &other) = delete;
  WatchedDirectory(WatchedDirectory &&other) = delete;
  WatchedDirectory &operator=(const WatchedDirectory &other) = delete;
  WatchedDirectory &operator=(WatchedDirectory &&other) = delete;

private:
  void build_absolute_path(std::ostringstream &stream);

  // Translate the relative path within an inotify event into an absolute path within this directory.
  std::string absolute_event_path(const inotify_event &event);

  int wd;
  ChannelID channel_id;
  std::shared_ptr<WatchedDirectory> parent;
  std::string name;
  bool recursive;
};

#endif
