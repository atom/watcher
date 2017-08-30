#ifndef WATCHER_REGISTRY_H
#define WATCHER_REGISTRY_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <sys/inotify.h>

#include "../../errable.h"
#include "../../result.h"
#include "../../message_buffer.h"
#include "cookie_jar.h"
#include "side_effect.h"
#include "watched_directory.h"

// Manage the set of open inotify watch descriptors.
class WatchRegistry : public Errable {
public:

  // Initialize inotify. Enter an error state if inotify initialization fails.
  WatchRegistry();

  // Stop inotify and release all kernel resources associated with it.
  ~WatchRegistry();

  // Begin watching a root path. If `recursive` is `true`, recursively watch all
  // subdirectories as well.
  //
  // `root` must name a directory if `recursive` is `true`.
  Result<> add(ChannelID channel_id, std::string root, bool recursive);

  // Uninstall inotify watchers used to deliver events on a specified channel.
  Result<> remove(ChannelID channel_id);

  // Interpret all inotify events created since the previous call to consume(), until the
  // read() call would block. Buffer messages corresponding to each inotify event. Use the
  // CookieJar to match pairs of rename events and the SideEffect to enqueue side effects.
  Result<> consume(MessageBuffer &messages, CookieJar &jar, SideEffect &side);

  // Return the file descriptor that should be polled to wake up when inotify events are
  // available.
  int get_read_fd() { return inotify_fd; }

private:
  int inotify_fd;
  std::unordered_multimap<int, std::shared_ptr<WatchedDirectory>> by_wd;
  std::unordered_multimap<ChannelID, std::shared_ptr<WatchedDirectory>> by_channel;
};

#endif
