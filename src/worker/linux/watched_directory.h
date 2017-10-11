#ifndef WATCHED_DIRECTORY
#define WATCHED_DIRECTORY

#include <string>
#include <sys/inotify.h>
#include <vector>

#include "../../message_buffer.h"
#include "../../result.h"
#include "cookie_jar.h"
#include "side_effect.h"

// Associate resources used to watch inotify events that are delivered with a single watch descriptor.
class WatchedDirectory
{
public:
  WatchedDirectory(int wd, ChannelID channel_id, std::string &&directory);
  ~WatchedDirectory() = default;

  // Interpret a single inotify event. Buffer messages, store or resolve rename Cookies from the CookieJar, and
  // enqueue SideEffects based on the event's mask.
  Result<> accept_event(MessageBuffer &buffer, CookieJar &jar, SideEffect &side, const inotify_event &event);

  // Access the watch descriptor that corresponds to this directory.
  int get_descriptor() { return wd; }

  WatchedDirectory(const WatchedDirectory &other) = delete;
  WatchedDirectory(WatchedDirectory &&other) = delete;
  WatchedDirectory &operator=(const WatchedDirectory &other) = delete;
  WatchedDirectory &operator=(WatchedDirectory &&other) = delete;

private:
  // Translate the relative path within an inotify event into an absolute path within this directory.
  std::string get_absolute_path(const inotify_event &event);

  int wd;
  ChannelID channel_id;
  std::string directory;
};

#endif
