#include <string>
#include <utility>
#include <memory>
#include <unordered_map>
#include <set>
#include <vector>
#include <iostream>
#include <sys/inotify.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

#include "../../log.h"
#include "../../result.h"
#include "../../message.h"
#include "../../message_buffer.h"
#include "../../helper/linux/helper.h"
#include "watched_directory.h"
#include "watch_registry.h"

using std::string;
using std::move;
using std::endl;
using std::shared_ptr;
using std::set;
using std::ostream;

static ostream &operator<<(ostream &out, const inotify_event *event)
{
  out << " wd=" << event->wd;

  out << " mask=( ";
  if (event->mask & IN_ACCESS) out << "IN_ACCESS ";
  if (event->mask & IN_ATTRIB) out << "IN_ATTRIB ";
  if (event->mask & IN_CLOSE_WRITE) out << "IN_CLOSE_WRITE ";
  if (event->mask & IN_CLOSE_NOWRITE) out << "IN_CLOSE_NOWRITE ";
  if (event->mask & IN_CREATE) out << "IN_CREATE ";
  if (event->mask & IN_DELETE) out << "IN_DELETE ";
  if (event->mask & IN_DELETE_SELF) out << "IN_DELETE_SELF ";
  if (event->mask & IN_MOVE_SELF) out << "IN_MOVE_SELF ";
  if (event->mask & IN_MOVED_FROM) out << "IN_MOVED_FROM ";
  if (event->mask & IN_MOVED_TO) out << "IN_MOVED_TO ";
  if (event->mask & IN_OPEN) out << "IN_OPEN ";
  if (event->mask & IN_IGNORED) out << "IN_IGNORED ";
  if (event->mask & IN_Q_OVERFLOW) out << "IN_Q_OVERFLOW ";
  if (event->mask & IN_UNMOUNT) out << "IN_UNMOUNT ";
  out << " ) cookie=" << event->cookie;
  out << " len=" << event->len;
  if (event->len > 0) {
    out << " name=" << event->name;
  }

  return out;
}

WatchRegistry::WatchRegistry() :
  Errable("inotify watcher registry")
{
  inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

  if (inotify_fd == -1) {
    report_error(errno_result("Unable to initialize inotify"));
  }
}

WatchRegistry::~WatchRegistry()
{
  if (inotify_fd > 0) {
    close(inotify_fd);
  }
}

Result<> WatchRegistry::add(ChannelID channel_id, string root, bool recursive)
{
  if (!is_healthy()) return health_err_result<>();

  uint32_t mask =  IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF |
      IN_MOVED_FROM | IN_MOVED_TO | IN_DONT_FOLLOW | IN_EXCL_UNLINK;
  if (recursive) mask |= IN_ONLYDIR;

  LOGGER << "Watching path [" << root << "]." << endl;
  int wd = inotify_add_watch(inotify_fd, root.c_str(), mask);
  if (wd == -1) {
    // TODO: signal a revert to polling on ENOSPC
    return errno_result("Unable to watch directory");
  }

  LOGGER << "Assigned watch descriptor " << wd << "." << endl;

  string root_dup(root);
  shared_ptr<WatchedDirectory> watched_dir(new WatchedDirectory(wd, channel_id, move(root_dup)));

  by_wd.insert({wd, watched_dir});
  by_channel.insert({channel_id, watched_dir});

  if (recursive) {
    LOGGER << "Recursing into directory." << endl;
    DIR *dir = opendir(root.c_str());
    if (dir == NULL) {
      int open_errno = errno;
      if (open_errno != EACCES && open_errno != ENOENT && open_errno != ENOTDIR) {
        return errno_result("Unable to recurse into directory " + root, open_errno);
      }
    } else {
      errno = 0;
      dirent *entry = readdir(dir);
      while (entry != NULL) {
        string basename(entry->d_name);

        LOGGER << "Processing entry " << basename << "." << endl;

        if (basename == "." || basename == "..") {
          entry = readdir(dir);
          continue;
        }
        string subdir = root + "/" + basename;

#ifndef _DIRENT_HAVE_D_TYPE
        if (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) {
          LOGGER << "Recursing into [" << subdir << "]." << endl;
          Result<> add_r = add(channel_id, subdir, true);
          if (add_r.is_error()) {
            LOGGER << "Unable to recurse into " << subdir << ": " << add_r << "." << endl;
          }
        }
#else
        LOGGER << "Recursing into [" << subdir << "]." << endl;
        Result<> add_r = add(channel_id, subdir, true);
        if (add_r.is_error()) {
          LOGGER << "Unable to recurse into " << subdir << ": " << add_r << "." << endl;
        }
#endif

        entry = readdir(dir);
      }
      if (errno != 0) {
        return errno_result("Unable to iterate entries of directory " + root);
      }
    }
  }

  return ok_result();
}

Result<> WatchRegistry::remove(ChannelID channel_id)
{
  if (!is_healthy()) return health_err_result<>();

  auto its = by_channel.equal_range(channel_id);
  set<int> wds;
  for (auto it = its.first; it != its.second; ++it) {
    wds.insert(it->second->get_descriptor());
  }

  by_channel.erase(channel_id);
  for (auto &&wd : wds) {
    by_wd.erase(wd);
  }

  LOGGER << "Channel " << channel_id << " has been unwatched." << endl;

  return ok_result();
}

Result<> WatchRegistry::consume(MessageBuffer &messages)
{
  if (!is_healthy()) return health_err_result<>();

  LOGGER << "Consuming inotify events." << endl;

  const size_t BUFSIZE = 2048 * sizeof(inotify_event);
  char buf[BUFSIZE] __attribute__ ((aligned(__alignof__(struct inotify_event))));
  ssize_t result = 0;

  while (true) {
    result = read(inotify_fd, &buf, BUFSIZE);

    if (result < 0) {
      int read_errno = errno;

      if (read_errno == EAGAIN || read_errno == EWOULDBLOCK) {
        // Nothing left to read.
        LOGGER << "Nothing left to read." << endl;
        return ok_result();
      }

      return errno_result<>("Unable to read inotify events", read_errno);
    }

    if (result == 0) {
      LOGGER << "EOF." << endl;
      return ok_result();
    }

    // At least one inotify event to read.
    char *current = buf;
    inotify_event *event = nullptr;
    while (current < buf + result) {
      event = reinterpret_cast<inotify_event*>(current);
      current += sizeof(inotify_event) + event->len;

      LOGGER << "Received inotify event: " << event << "." << endl;

      if (event->mask & IN_Q_OVERFLOW) {
        LOGGER << "Event queue overflow. Some events have been missed." << endl;
        continue;
      }

      auto its = by_wd.equal_range(event->wd);
      if (its.first == by_wd.end() && its.second == by_wd.end()) {
        LOGGER << "Received event for unknown watch descriptor " << event->wd << "." << endl;
        continue;
      }

      for (auto it = its.first; it != its.second; ++it) {
        shared_ptr<WatchedDirectory> watched_directory = it->second;

        Result<> r = watched_directory->accept_event(messages, *event);
        if (r.is_error()) {
          LOGGER << "Unable to process event: " << r << "." << endl;
        }
      }
    }
  }
}
