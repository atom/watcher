#include <memory>
#include <sstream>
#include <string>
#include <sys/inotify.h>
#include <utility>

#include "../../message.h"
#include "../../message_buffer.h"
#include "../../result.h"
#include "../recent_file_cache.h"
#include "cookie_jar.h"
#include "side_effect.h"
#include "watched_directory.h"

using std::move;
using std::ostringstream;
using std::shared_ptr;
using std::string;

WatchedDirectory::WatchedDirectory(int wd,
  ChannelID channel_id,
  shared_ptr<WatchedDirectory> parent,
  string &&name,
  bool recursive) :
  wd{wd},
  channel_id{channel_id},
  parent{parent},
  name{move(name)},
  recursive{recursive}
{
  //
}

Result<> WatchedDirectory::accept_event(MessageBuffer &buffer,
  CookieJar &jar,
  SideEffect &side,
  RecentFileCache &cache,
  const inotify_event &event)
{
  string basename{event.name};
  string path = absolute_event_path(event);

  bool dir_hint = (event.mask & IN_ISDIR) == IN_ISDIR;

  // Read or refresh the cached lstat() entry primarily to determine if this entry is a symlink or not.
  shared_ptr<StatResult> stat = cache.former_at_path(path, !dir_hint, dir_hint, false);
  if (stat->is_absent()) {
    stat = cache.current_at_path(path, !dir_hint, dir_hint, false);
    cache.apply();
  }
  EntryKind kind = stat->get_entry_kind();

  if ((event.mask & IN_CREATE) == IN_CREATE) {
    // create entry inside directory
    if (kind == KIND_DIRECTORY && recursive) {
      side.track_subdirectory(basename, channel_id);
    }
    buffer.created(channel_id, move(path), kind);
    return ok_result();
  }

  if ((event.mask & IN_DELETE) == IN_DELETE) {
    // delete entry inside directory
    cache.evict(path);
    buffer.deleted(channel_id, move(path), kind);
    return ok_result();
  }

  if ((event.mask & (IN_MODIFY | IN_ATTRIB)) != 0u) {
    // modify entry inside directory or attribute change for directory or entry inside directory
    buffer.modified(channel_id, move(path), kind);
    return ok_result();
  }

  if ((event.mask & (IN_DELETE_SELF | IN_UNMOUNT)) != 0u) {
    if (is_root()) {
      side.remove_channel(channel_id);
      cache.evict(get_absolute_path());
      buffer.deleted(channel_id, get_absolute_path(), KIND_DIRECTORY);
    }
    return ok_result();
  }

  if ((event.mask & IN_MOVE_SELF) == IN_MOVE_SELF) {
    // directory itself was renamed
    if (is_root()) {
      side.remove_channel(channel_id);
      cache.evict(get_absolute_path());
      buffer.deleted(channel_id, get_absolute_path(), KIND_DIRECTORY);
    }
    return ok_result();
  }

  if ((event.mask & IN_MOVED_FROM) == IN_MOVED_FROM) {
    // rename source for directory or entry inside directory
    cache.evict(path);
    jar.moved_from(buffer, channel_id, event.cookie, move(path), kind);
    return ok_result();
  }

  if ((event.mask & IN_MOVED_TO) == IN_MOVED_TO) {
    // rename destination for directory or entry inside directory
    if (kind == KIND_DIRECTORY && recursive) {
      side.track_subdirectory(basename, channel_id);
    }
    jar.moved_to(buffer, channel_id, event.cookie, move(path), kind);
    return ok_result();
  }

  // IN_IGNORED

  return ok_result();
}

string WatchedDirectory::get_absolute_path()
{
  ostringstream stream;
  build_absolute_path(stream);
  return stream.str();
}

void WatchedDirectory::build_absolute_path(ostringstream &stream)
{
  if (parent) {
    parent->build_absolute_path(stream);
    stream << "/";
  }
  stream << name;
}

string WatchedDirectory::absolute_event_path(const inotify_event &event)
{
  ostringstream stream;
  build_absolute_path(stream);
  if (event.len > 0) {
    stream << "/" << event.name;
  }
  return stream.str();
}
