#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <string>
#include <vector>
#include <CoreServices/CoreServices.h>

#include "recent_file_cache.h"
#include "rename_buffer.h"
#include "../../message.h"
#include "../../message_buffer.h"

class EventHandler {
public:
  EventHandler(ChannelMessageBuffer &message_buffer, RecentFileCache &cache);
  EventHandler(const EventHandler &) = delete;
  EventHandler(EventHandler &&) = delete;

  void handle(std::string &event_path, FSEventStreamEventFlags flags);
  void flush();

private:
  RecentFileCache &cache;
  ChannelMessageBuffer &message_buffer;
  RenameBuffer rename_buffer;

  friend class EventFunctor;
};

#endif
