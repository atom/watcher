#include <cerrno>
#include <dirent.h>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "../../helper/linux/helper.h"
#include "../../log.h"
#include "../../message.h"
#include "../../message_buffer.h"
#include "../../result.h"
#include "../recent_file_cache.h"
#include "cookie_jar.h"
#include "side_effect.h"
#include "watch_registry.h"
#include "watched_directory.h"

using std::endl;
using std::ostream;
using std::ostringstream;
using std::set;
using std::shared_ptr;
using std::string;
using std::unordered_multimap;
using std::vector;

using WatchedDirectoryPtr = shared_ptr<WatchedDirectory>;
using WDMap = unordered_multimap<int, WatchedDirectoryPtr>;
using WDIter = WDMap::iterator;

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

WatchRegistry::WatchRegistry()
{
  inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

  if (inotify_fd == -1) {
    report_if_error(errno_result("Unable to initialize inotify"));
  }
  freeze();
}

WatchRegistry::~WatchRegistry()
{
  if (inotify_fd > 0) {
    close(inotify_fd);
  }
}

Result<> WatchRegistry::add(ChannelID channel_id,
  const shared_ptr<WatchedDirectory> &parent,
  const string &name,
  bool recursive,
  vector<string> &poll)
{
  uint32_t mask = IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF | IN_MOVED_FROM
    | IN_MOVED_TO | IN_DONT_FOLLOW | IN_EXCL_UNLINK | IN_ONLYDIR;

  ostringstream absolute_builder;
  if (parent) {
    absolute_builder << parent->get_absolute_path() << "/";
  }
  absolute_builder << name;
  string absolute = absolute_builder.str();

  ostream &logline = LOGGER << "Watching path [" << absolute << "]";
  if (!recursive) logline << " (non-recursively)";
  logline << "." << endl;

  int wd = inotify_add_watch(inotify_fd, absolute.c_str(), mask);
  if (wd == -1) {
    int watch_errno = errno;

    if (watch_errno == ENOENT || watch_errno == EACCES) {
      LOGGER << "Directory " << absolute << " is no longer accessible. Ignoring." << endl;
      return ok_result();
    }

    if (watch_errno == ENOSPC) {
      LOGGER << "Falling back to polling for directory " << absolute << "." << endl;
      poll.push_back(absolute);
      return ok_result();
    }

    return errno_result("Unable to watch directory", watch_errno);
  }

  LOGGER << "Assigned watch descriptor " << wd << " at [" << absolute << "] on channel " << channel_id << "." << endl;

  auto range = by_wd.equal_range(wd);
  bool updated = false;
  for (auto existing = range.first; existing != range.second; ++existing) {
    shared_ptr<WatchedDirectory> &other = existing->second;
    if (other->get_channel_id() == channel_id) {
      assert(parent != nullptr);
      updated = true;
      other->was_renamed(parent, name);
    }
  }
  if (updated) return ok_result();

  shared_ptr<WatchedDirectory> watched_dir(new WatchedDirectory(wd, channel_id, parent, string(name), recursive));

  by_wd.emplace(wd, watched_dir);
  by_channel.emplace(channel_id, watched_dir);

  if (recursive) {
    DIR *dir = opendir(absolute.c_str());
    if (dir == nullptr) {
      int open_errno = errno;
      if (open_errno != EACCES && open_errno != ENOENT && open_errno != ENOTDIR) {
        return errno_result("Unable to recurse into directory " + absolute, open_errno);
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

#ifdef _DIRENT_HAVE_D_TYPE
        if (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) {
          Result<> add_r = add(channel_id, watched_dir, basename, recursive, poll);
          if (add_r.is_error()) {
            LOGGER << "Unable to recurse into " << absolute << "/" << basename << ": " << add_r << "." << endl;
          }
        }
#else
        Result<> add_r = add(channel_id, watched_dir, basename, recursive, poll);
        if (add_r.is_error()) {
          LOGGER << "Unable to recurse into " << absolute << "/" << basename << ": " << add_r << "." << endl;
        }
#endif

        errno = 0;
        entry = readdir(dir);
      }
      if (errno != 0) {
        return errno_result("Unable to iterate entries of directory " + absolute);
      }
      closedir(dir);
    }
  }

  return ok_result();
}

Result<> WatchRegistry::remove(ChannelID channel_id)
{
  auto its = by_channel.equal_range(channel_id);
  set<int> wds;
  for (auto it = its.first; it != its.second; ++it) {
    wds.insert(it->second->get_descriptor());
  }

  LOGGER << "Stopping " << plural(wds.size(), "inotify watch descriptor") << "." << endl;

  by_channel.erase(channel_id);
  for (auto &wd : wds) {
    auto wd_matches = by_wd.equal_range(wd);

    vector<WDIter> to_erase;
    for (auto each_wd = wd_matches.first; each_wd != wd_matches.second; ++each_wd) {
      if (each_wd->second->get_channel_id() == channel_id) {
        to_erase.push_back(each_wd);
      }
    }
    for (WDIter &it : to_erase) {
      by_wd.erase(it);
    }

    if (by_wd.count(wd) == 0) {
      int err = inotify_rm_watch(inotify_fd, wd);
      if (err == -1) {
        LOGGER << "Unable to remove watch descriptor " << wd << ": " << errno_result<>("") << "." << endl;
      }
    }
  }

  LOGGER << "Channel " << channel_id << " has been unwatched." << endl;
  return ok_result();
}

Result<> WatchRegistry::consume(MessageBuffer &messages, CookieJar &jar, RecentFileCache &cache)
{
  Timer t;
  const size_t BUFSIZE = 2048 * sizeof(inotify_event);
  char buf[BUFSIZE] __attribute__((aligned(__alignof__(struct inotify_event))));
  ssize_t result = 0;
  size_t batch_count = 0;
  size_t event_count = 0;

  while (true) {
    result = read(inotify_fd, &buf, BUFSIZE);

    if (result <= 0) {
      jar.flush_oldest_batch(messages, cache);

      t.stop();
      LOGGER << plural(batch_count, "filesystem event batch", "filesystem event batches") << " containing "
             << plural(event_count, "event") << " completed. " << plural(messages.size(), "message") << " produced in "
             << t << "." << endl;
    }

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
    batch_count++;
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

      event_count++;

      vector<shared_ptr<WatchedDirectory>> watched_directories;
      for (auto it = its.first; it != its.second; ++it) {
        watched_directories.emplace_back(it->second);
      }

      for (shared_ptr<WatchedDirectory> &watched_directory : watched_directories) {
        SideEffect side;
        Result<> r = watched_directory->accept_event(messages, jar, side, cache, *event);
        if (r.is_error()) LOGGER << "Unable to process event: " << r << "." << endl;
        side.enact_in(watched_directory, this, messages);
      }
    }
  }
}
