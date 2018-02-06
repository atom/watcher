#ifndef WATCHER_REGISTRY_H
#define WATCHER_REGISTRY_H

#include <memory>
#include <string>
#include <sys/inotify.h>
#include <unordered_map>
#include <vector>

#include "../../errable.h"
#include "../../message_buffer.h"
#include "../../result.h"
#include "../recent_file_cache.h"
#include "cookie_jar.h"
#include "side_effect.h"
#include "watched_directory.h"

// Manage the set of open inotify watch descriptors.
class WatchRegistry : public Errable
{
public:
  // Initialize inotify. Enter an error state if inotify initialization fails.
  WatchRegistry();

  // Stop inotify and release all kernel resources associated with it.
  ~WatchRegistry() override;

  // Begin watching a root path. If `recursive` is `true`, recursively watch all subdirectories as well. If inotify
  // watch descriptors are exhausted before the entire directory tree can be watched, the unsuccessfully watched roots
  // will be accumulated into the `poll` vector.
  //
  // `root` must name a directory if `recursive` is `true`.
  Result<> add(ChannelID channel_id, const std::string &root, bool recursive, std::vector<std::string> &poll)
  {
    return add(channel_id, nullptr, root, recursive, poll);
  }

  // Begin watching path beneath an existing WatchedDirectory. If `recursive` is `true`, recursively watch all
  // subdirectories as well. If inotify watch descriptors are exhausted before the entire directory tree can be watched,
  // the unsuccessfully watched roots will be accumulated into the `poll` vector.
  //
  // `root` must name a directory if `recursive` is `true`.
  Result<> add(ChannelID channel_id,
    const std::shared_ptr<WatchedDirectory> &parent,
    const std::string &name,
    bool recursive,
    std::vector<std::string> &poll);

  // Uninstall inotify watchers used to deliver events on a specified channel.
  Result<> remove(ChannelID channel_id);

  // Interpret all inotify events created since the previous call to consume(), until the
  // read() call would block. Buffer messages corresponding to each inotify event. Use the
  // CookieJar to match pairs of rename events across event batches and the RecentFileCache to
  // identify symlinks without doing a stat for every event.
  Result<> consume(MessageBuffer &messages, CookieJar &jar, RecentFileCache &cache);

  // Return the file descriptor that should be polled to wake up when inotify events are
  // available.
  int get_read_fd() { return inotify_fd; }

  WatchRegistry(const WatchRegistry &) = delete;
  WatchRegistry(WatchRegistry &&) = delete;
  WatchRegistry &operator=(const WatchRegistry &) = delete;
  WatchRegistry &operator=(WatchRegistry &&) = delete;

private:
  int inotify_fd;
  std::unordered_multimap<int, std::shared_ptr<WatchedDirectory>> by_wd;
  std::unordered_multimap<ChannelID, std::shared_ptr<WatchedDirectory>> by_channel;
};

#endif
