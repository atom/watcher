#include <string>
#include <utility>
#include <vector>
#include <sys/inotify.h>

#include "../../result.h"
#include "../../message.h"
#include "../../message_buffer.h"
#include "watched_directory.h"

using std::string;
using std::vector;
using std::move;

WatchedDirectory::WatchedDirectory(int wd, ChannelID channel_id, string &&directory) :
  wd{wd},
  channel_id{channel_id},
  directory{move(directory)}
{
  //
}

Result<> WatchedDirectory::accept_event(MessageBuffer &buffer, const inotify_event &event)
{
  EntryKind kind = event.mask & IN_ISDIR ? KIND_DIRECTORY : KIND_FILE;
  string path = get_absolute_path(event);

  if (event.mask & IN_CREATE) {
    // create entry inside directory

    if (kind == KIND_DIRECTORY) {
      // subdirectory created
      // TODO: create subdirectory watcher
      buffer.created(channel_id, move(path), kind);
      return ok_result();
    } else {
      // file created
      buffer.created(channel_id, move(path), kind);
      return ok_result();
    }
  }

  if (event.mask & IN_DELETE) {
    // delete entry inside directory
    buffer.deleted(channel_id, move(path), kind);
    return ok_result();
  }

  if (event.mask & (IN_MODIFY | IN_ATTRIB)) {
    // modify entry inside directory or attribute change for directory or entry inside directory
    buffer.modified(channel_id, move(path), kind);
  }

  if (event.mask & (IN_DELETE_SELF | IN_UNMOUNT)) {
    buffer.deleted(channel_id, move(path), kind);
  }

  if (event.mask & IN_MOVE_SELF) {
    // directory itself was renamed
    // TODO note old name and cookie
  }

  if (event.mask & IN_MOVED_FROM) {
    // rename source for directory or entry inside directory
    // TODO note new name and cookie
  }

  if (event.mask & IN_MOVED_TO) {
    // rename destination for directory or entry inside directory
    // TODO note new name and cookie
  }

  // IN_IGNORED

  return ok_result();
}

string WatchedDirectory::get_absolute_path(const inotify_event &event)
{
  if (event.len == 0) {
    // Return a copy because the path gets moved
    return string(directory);
  }

  return string(directory + "/" + event.name);
}
