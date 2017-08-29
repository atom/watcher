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
#include "watched_directory.h"

class WatchRegistry : public Errable {
public:
  WatchRegistry();
  ~WatchRegistry();

  Result<> add(ChannelID channel_id, std::string root, bool recursive);

  Result<> remove(ChannelID channel_id);

  Result<> consume(MessageBuffer &messages);

  int get_read_fd() { return inotify_fd; }

private:
  int inotify_fd;
  std::unordered_multimap<int, std::shared_ptr<WatchedDirectory>> by_wd;
  std::unordered_multimap<ChannelID, std::shared_ptr<WatchedDirectory>> by_channel;
};

#endif
