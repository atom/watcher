#include <cerrno>
#include <dirent.h>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../helper/linux/helper.h"
#include "../../log.h"
#include "../../message.h"
#include "../../message_buffer.h"
#include "../../result.h"
#include "cookie_jar.h"
#include "side_effect.h"
#include "watch_registry.h"
#include "watched_directory.h"

using std::endl;
using std::move;
using std::ostream;
using std::set;
using std::shared_ptr;
using std::string;

static ostream &operator<<(ostream &out, const inotify_event *event)
{
  out << "wd=" << event->wd;

  out << " mask=( ";
  if ((event->mask & IN_ACCESS) == IN_ACCESS) out << "IN_ACCESS ";
  if ((event->mask & IN_ATTRIB) == IN_ATTRIB) out << "IN_ATTRIB ";
  if ((event->mask & IN_CLOSE_WRITE) == IN_CLOSE_WRITE) out << "IN_CLOSE_WRITE ";
  if ((event->mask & IN_CLOSE_NOWRITE) == IN_CLOSE_NOWRITE) out << "IN_CLOSE_NOWRITE ";
  if ((event->mask & IN_CREATE) == IN_CREATE) out << "IN_CREATE ";
  if ((event->mask & IN_DELETE) == IN_DELETE) out << "IN_DELETE ";
  if ((event->mask & IN_DELETE_SELF) == IN_DELETE_SELF) out << "IN_DELETE_SELF ";
  if ((event->mask & IN_MODIFY) == IN_MODIFY) out << "IN_MODIFY ";
  if ((event->mask & IN_MOVE_SELF) == IN_MOVE_SELF) out << "IN_MOVE_SELF ";
  if ((event->mask & IN_MOVED_FROM) == IN_MOVED_FROM) out << "IN_MOVED_FROM ";
  if ((event->mask & IN_MOVED_TO) == IN_MOVED_TO) out << "IN_MOVED_TO ";
  if ((event->mask & IN_OPEN) == IN_OPEN) out << "IN_OPEN ";
  if ((event->mask & IN_IGNORED) == IN_IGNORED) out << "IN_IGNORED ";
  if ((event->mask & IN_Q_OVERFLOW) == IN_Q_OVERFLOW) out << "IN_Q_OVERFLOW ";
  if ((event->mask & IN_UNMOUNT) == IN_UNMOUNT) out << "IN_UNMOUNT ";
  if ((event->mask & IN_ISDIR) == IN_ISDIR) out << "IN_ISDIR ";
  out << ") cookie=" << event->cookie;
  out << " len=" << event->len;
  if (event->len > 0) {
    out << " name=" << event->name;
  }

  return out;
}

WatchRegistry::WatchRegistry() : Errable("inotify watcher registry")
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

Result<> WatchRegistry::add(ChannelID channel_id, const string &root, bool recursive)
{
  if (!is_healthy()) return health_err_result<>();

  uint32_t mask = IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF | IN_MOVED_FROM
    | IN_MOVED_TO | IN_DONT_FOLLOW | IN_EXCL_UNLINK;
  if (recursive) mask |= IN_ONLYDIR;

  LOGGER << "Watching path [" << root << "]." << endl;
  int wd = inotify_add_watch(inotify_fd, root.c_str(), mask);
  if (wd == -1) {
    int watch_errno = errno;

    if (watch_errno == ENOTDIR) {
      return ok_result();
    }

    // TODO: signal a revert to polling on ENOSPC
    return errno_result("Unable to watch directory", watch_errno);
  }

  LOGGER << "Assigned watch descriptor " << wd << " at [" << root << "] on channel " << channel_id << "." << endl;

  shared_ptr<WatchedDirectory> watched_dir(new WatchedDirectory(wd, channel_id, string(root)));

  by_wd.insert({wd, watched_dir});
  by_channel.insert({channel_id, watched_dir});

  if (recursive) {
    DIR *dir = opendir(root.c_str());
    if (dir == nullptr) {
      int open_errno = errno;
      if (open_errno != EACCES && open_errno != ENOENT && open_errno != ENOTDIR) {
        return errno_result("Unable to recurse into directory " + root, open_errno);
      }
    } else {
      errno = 0;
      dirent *entry = readdir(dir);
      while (entry != nullptr) {
        string basename(entry->d_name);

        if (basename == "." || basename == "..") {
          entry = readdir(dir);
          continue;
        }
        string subdir(root);
        subdir += "/";
        subdir += basename;

#ifndef _DIRENT_HAVE_D_TYPE
        if (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) {
          Result<> add_r = add(channel_id, subdir, true);
          if (add_r.is_error()) {
            LOGGER << "Unable to recurse into " << subdir << ": " << add_r << "." << endl;
          }
        }
#else
        Result<> add_r = add(channel_id, subdir, true);
        if (add_r.is_error()) {
          LOGGER << "Unable to recurse into " << subdir << ": " << add_r << "." << endl;
        }
#endif

        errno = 0;
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

  LOGGER << "Stopping " << plural(wds.size(), "inotify watch descriptor") << "." << endl;

  by_channel.erase(channel_id);
  for (auto &&wd : wds) {
    by_wd.erase(wd);

    int err = inotify_rm_watch(inotify_fd, wd);
    if (err == -1) {
      LOGGER << "Unable to remove watch descriptor " << wd << ": " << errno_result<>("") << "." << endl;
    }
  }

  LOGGER << "Channel " << channel_id << " has been unwatched." << endl;
  return ok_result();
}

Result<> WatchRegistry::consume(MessageBuffer &messages, CookieJar &jar, SideEffect &side)
{
  if (!is_healthy()) return health_err_result<>();

  const size_t BUFSIZE = 2048 * sizeof(inotify_event);
  char buf[BUFSIZE] __attribute__((aligned(__alignof__(struct inotify_event))));
  ssize_t result = 0;

  while (true) {
    result = read(inotify_fd, &buf, BUFSIZE);

    if (result <= 0) jar.flush_oldest_batch(messages);

    if (result < 0) {
      int read_errno = errno;

      if (read_errno == EAGAIN || read_errno == EWOULDBLOCK) {
        // Nothing left to read.
        return ok_result();
      }

      return errno_result<>("Unable to read inotify events", read_errno);
    }

    if (result == 0) {
      return ok_result();
    }

    // At least one inotify event to read.
    char *current = buf;
    inotify_event *event = nullptr;
    while (current < buf + result) {
      event = reinterpret_cast<inotify_event *>(current);
      current += sizeof(inotify_event) + event->len;

      LOGGER << "Received inotify event: " << event << "." << endl;

      if ((event->mask & IN_Q_OVERFLOW) == IN_Q_OVERFLOW) {
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

        Result<> r = watched_directory->accept_event(messages, jar, side, *event);
        if (r.is_error()) {
          LOGGER << "Unable to process event: " << r << "." << endl;
        }
      }
    }
  }
}
