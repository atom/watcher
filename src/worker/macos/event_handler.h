#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <CoreServices/CoreServices.h>
#include <string>
#include <vector>

#include "../../message.h"
#include "../../message_buffer.h"
#include "recent_file_cache.h"
#include "rename_buffer.h"

class EventHandler
{
public:
  EventHandler(ChannelMessageBuffer &message_buffer, RecentFileCache &cache, RenameBuffer &rename_buffer);

  ~EventHandler() = default;

  void handle(std::string &&event_path, FSEventStreamEventFlags flags);

  EventHandler(const EventHandler &) = delete;
  EventHandler(EventHandler &&) = delete;
  EventHandler &operator=(const EventHandler &) = delete;
  EventHandler &operator=(EventHandler &&) = delete;

private:
  RecentFileCache &cache;
  ChannelMessageBuffer &message_buffer;
  RenameBuffer &rename_buffer;

  friend class EventFunctor;
};

#endif
