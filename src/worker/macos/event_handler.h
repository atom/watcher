#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <string>
#include <vector>
#include <CoreServices/CoreServices.h>

#include "recent_file_cache.h"
#include "../../message.h"

class EventHandler {
public:
  EventHandler(std::vector<Message> &messages, RecentFileCache &cache, ChannelID channel_id);
  EventHandler(const EventHandler &) = delete;
  EventHandler(EventHandler &&) = delete;

  void handle(std::string &event_path, FSEventStreamEventFlags flags);

private:
  void enqueue_creation(std::string event_path, const EntryKind &kind);
  void enqueue_modification(std::string event_path, const EntryKind &kind);
  void enqueue_deletion(std::string event_path, const EntryKind &kind);
  void enqueue_rename(std::string old_path, std::string new_path, const EntryKind &kind);

  std::vector<Message> &messages;
  RecentFileCache &cache;
  const ChannelID channel_id;

  std::string rename_old_path;

  friend class EventFunctor;
};

#endif
