#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <CoreServices/CoreServices.h>
#include <string>
#include <vector>

#include "../../message.h"
#include "../../message_buffer.h"
#include "flags.h"
#include "recent_file_cache.h"
#include "rename_buffer.h"

class BatchHandler
{
public:
  BatchHandler(ChannelMessageBuffer &message_buffer,
    RecentFileCache &cache,
    RenameBuffer &rename_buffer,
    bool recursive,
    const std::string &root_path);

  ~BatchHandler() = default;

  void event(std::string &&event_path, FSEventStreamEventFlags flags);

  BatchHandler(const BatchHandler &) = delete;
  BatchHandler(BatchHandler &&) = delete;
  BatchHandler &operator=(const BatchHandler &) = delete;
  BatchHandler &operator=(BatchHandler &&) = delete;

private:
  RecentFileCache &cache;
  ChannelMessageBuffer &message_buffer;
  RenameBuffer &rename_buffer;
  bool recursive;
  const std::string &root_path;

  friend class Event;
};

class Event
{
public:
  ~Event() = default;

  bool flag_created() { return (flags & CREATE_FLAGS) != 0; }

  bool flag_deleted() { return (flags & DELETED_FLAGS) != 0; }

  bool flag_modified() { return (flags & MODIFY_FLAGS) != 0; }

  bool flag_renamed() { return (flags & RENAME_FLAGS) != 0; }

  bool flag_file() { return (flags & IS_FILE) != 0; }

  bool flag_directory() { return (flags & IS_DIRECTORY) != 0; }

  bool is_recursive() { return handler.recursive; }

  const std::string &root_path() { return handler.root_path; }

  ChannelMessageBuffer &message_buffer() { return handler.message_buffer; }

  RecentFileCache &cache() { return handler.cache; }

  RenameBuffer &rename_buffer() { return handler.rename_buffer; }

  const std::shared_ptr<StatResult> &get_former() { return former; }

  const std::shared_ptr<StatResult> &get_current() { return current; }

  Event(const Event &) = delete;
  Event(Event &&) = delete;
  Event &operator=(const Event &) = delete;
  Event &operator=(Event &&) = delete;

private:
  Event(BatchHandler &batch, std::string &&event_path, FSEventStreamEventFlags flags);

  bool skip_recursive_event();

  void collect_info();

  void report();

  bool emit_if_unambiguous();

  bool emit_if_rename();

  bool emit_if_absent();

  bool emit_if_present();

  BatchHandler &handler;
  std::string event_path;
  FSEventStreamEventFlags flags;

  std::shared_ptr<StatResult> former;
  std::shared_ptr<StatResult> current;

  friend class BatchHandler;
};

#endif
