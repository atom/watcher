#ifndef WATCHED_DIRECTORY
#define WATCHED_DIRECTORY

#include <string>
#include <vector>
#include <sys/inotify.h>

#include "../../result.h"
#include "../../message_buffer.h"
#include "side_effect.h"
#include "cookie_jar.h"

class WatchedDirectory {
public:
  WatchedDirectory(int wd, ChannelID channel_id, std::string &&directory);

  Result<> accept_event(MessageBuffer &buffer, CookieJar &jar, SideEffect &side, const inotify_event &event);

  int get_descriptor() { return wd; }

private:
  WatchedDirectory(const WatchedDirectory &other) = delete;
  WatchedDirectory(WatchedDirectory &&other) = delete;
  WatchedDirectory &operator=(const WatchedDirectory &other) = delete;
  WatchedDirectory &operator=(WatchedDirectory &&other) = delete;

  std::string get_absolute_path(const inotify_event &event);

  int wd;
  ChannelID channel_id;
  std::string directory;
};

#endif
