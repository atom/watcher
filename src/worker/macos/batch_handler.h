#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <CoreServices/CoreServices.h>
#include <list>
#include <string>

#include "../../message.h"
#include "../../message_buffer.h"
#include "../recent_file_cache.h"
#include "flags.h"
#include "rename_buffer.h"

class BatchHandler;

class Event
{
public:
  Event(BatchHandler &batch, std::string &&event_path, FSEventStreamEventFlags flags);

  Event(Event &&original) noexcept;

  ~Event() = default;

  bool flag_created() { return (flags & CREATE_FLAGS) != 0; }

  bool flag_deleted() { return (flags & DELETED_FLAGS) != 0; }

  bool flag_modified() { return (flags & MODIFY_FLAGS) != 0; }

  bool flag_renamed() { return (flags & RENAME_FLAGS) != 0; }

  bool flag_file() { return (flags & IS_FILE) != 0; }

  bool flag_directory() { return (flags & IS_DIRECTORY) != 0; }

  bool flag_symlink() { return (flags & IS_SYMLINK) != 0; }

  bool is_recursive();

  const std::string &root_path();

  ChannelMessageBuffer &message_buffer();

  RecentFileCache &cache();

  RenameBuffer &rename_buffer();

  const std::string &get_event_path() { return event_path; }

  const std::string &get_stat_path() { return updated_event_path.empty() ? event_path : updated_event_path; }

  const std::shared_ptr<StatResult> &get_former() { return former; }

  const std::shared_ptr<StatResult> &get_current() { return current; }

  bool update_for_rename(const std::string &from_dir_path, const std::string &to_dir_path);

  bool needs_updated_info() { return !current || (get_stat_path() != current->get_path()); };

  Event(const Event &) = delete;
  Event &operator=(const Event &) = delete;
  Event &operator=(Event &&) = delete;

private:
  bool skip_recursive_event();

  void collect_info();

  bool should_defer();

  void report();

  bool emit_if_unambiguous();

  bool emit_if_rename();

  bool emit_if_absent();

  bool emit_if_present();

  BatchHandler &handler;
  std::string event_path;
  std::string updated_event_path;
  FSEventStreamEventFlags flags;

  std::shared_ptr<StatResult> former;
  std::shared_ptr<StatResult> current;

  friend class BatchHandler;
};

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

  bool update_for_rename(const std::string &from_dir_path, const std::string &to_dir_path);

  void handle_deferred();

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
  std::list<Event> deferred;

  friend class Event;
};

// Inline event methods that need the full BatchHandler type.

inline bool Event::is_recursive()
{
  return handler.recursive;
}

inline const std::string &Event::root_path()
{
  return handler.root_path;
}

inline ChannelMessageBuffer &Event::message_buffer()
{
  return handler.message_buffer;
}

inline RecentFileCache &Event::cache()
{
  return handler.cache;
}

inline RenameBuffer &Event::rename_buffer()
{
  return handler.rename_buffer;
}

#endif
