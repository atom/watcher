#include "event_handler.h"

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <iostream>
#include <iomanip>
#include <errno.h>
#include <sys/stat.h>
#include <CoreServices/CoreServices.h>

#include "flags.h"
#include "recent_file_cache.h"
#include "rename_buffer.h"
#include "../../message.h"
#include "../../log.h"

using std::vector;
using std::string;
using std::shared_ptr;
using std::move;
using std::ostream;
using std::hex;
using std::dec;
using std::endl;

class EventFunctor {
public:
  EventFunctor(EventHandler &handler, string &event_path, FSEventStreamEventFlags flags) :
    handler{handler},
    cache{handler.cache},
    rename_buffer{handler.rename_buffer},
    event_path{event_path},
    flags{flags},
    stat_performed{false}
  {
    flag_created = (flags & CREATE_FLAGS) != 0;
    flag_deleted = (flags & DELETED_FLAGS) != 0;
    flag_modified = (flags & MODIFY_FLAGS) != 0;
    flag_renamed = (flags & RENAME_FLAGS) != 0;
    flag_file = (flags & IS_FILE) != 0;
    flag_directory = (flags & IS_DIRECTORY) != 0;
  }

  void operator()()
  {
    report();
    determine_entry_kinds();
    LOGGER
      << "Entry kinds: current_kind=" << current_kind
      << " former_kind=" << former_kind << "." << endl;
    check_cache();
    LOGGER
      << "Cache status: seen_before=" << seen_before
      << " was_different_entry=" << was_different_entry << "." << endl;

    if (emit_if_unambiguous()) return;
    if (emit_if_absent()) return;
    emit_if_present();
  }

private:
  // Log the raw and interpreted flag information.
  void report()
  {
    ostream &logline = LOGGER;
    logline << "Native event received at [" << event_path << "]:" << endl;

    logline << "  event flags " << hex << flags << dec << " =";
    if ((flags & kFSEventStreamEventFlagMustScanSubDirs) != 0) logline << " MustScanSubDirs";
    if ((flags & kFSEventStreamEventFlagUserDropped) != 0) logline << " UserDropped";
    if ((flags & kFSEventStreamEventFlagKernelDropped) != 0) logline << " KernelDropped";
    if ((flags & kFSEventStreamEventFlagEventIdsWrapped) != 0) logline << " EventIdsWrapped";
    if ((flags & kFSEventStreamEventFlagMustScanSubDirs) != 0) logline << " MustScanSubDirs";
    if ((flags & kFSEventStreamEventFlagHistoryDone) != 0) logline << " HistoryDone";
    if ((flags & kFSEventStreamEventFlagRootChanged) != 0) logline << " RootChanged";
    if ((flags & kFSEventStreamEventFlagMount) != 0) logline << " Mount";
    if ((flags & kFSEventStreamEventFlagUnmount) != 0) logline << " Unmount";
    if ((flags & kFSEventStreamEventFlagMustScanSubDirs) != 0) logline << " MustScanSubDirs";
    if ((flags & kFSEventStreamEventFlagItemCreated) != 0) logline << " ItemCreated";
    if ((flags & kFSEventStreamEventFlagItemRemoved) != 0) logline << " ItemRemoved";
    if ((flags & kFSEventStreamEventFlagItemInodeMetaMod) != 0) logline << " ItemInodeMetaMod";
    if ((flags & kFSEventStreamEventFlagItemRenamed) != 0) logline << " ItemRenamed";
    if ((flags & kFSEventStreamEventFlagItemModified) != 0) logline << " ItemModified";
    if ((flags & kFSEventStreamEventFlagItemFinderInfoMod) != 0) logline << " ItemFinderInfoMod";
    if ((flags & kFSEventStreamEventFlagItemChangeOwner) != 0) logline << " ItemChangeOwner";
    if ((flags & kFSEventStreamEventFlagItemXattrMod) != 0) logline << " ItemXattrMod";
    if ((flags & kFSEventStreamEventFlagItemIsFile) != 0) logline << " ItemIsFile";
    if ((flags & kFSEventStreamEventFlagItemIsDir) != 0) logline << " ItemIsDir";
    if ((flags & kFSEventStreamEventFlagItemIsSymlink) != 0) logline << " ItemIsSymlink";
    if ((flags & kFSEventStreamEventFlagOwnEvent) != 0) logline << " OwnEvent";
    if ((flags & kFSEventStreamEventFlagItemIsHardlink) != 0) logline << " ItemIsHardlink";
    if ((flags & kFSEventStreamEventFlagItemIsLastHardlink) != 0) logline << " ItemIsLastHardlink";
    logline << endl;

    logline << "  interpreted as"
      << " file=" << flag_file
      << " directory=" << flag_directory
      << " created=" << flag_created
      << " deleted=" << flag_deleted
      << " modified=" << flag_modified
      << " renamed=" << flag_renamed
      << endl;
  }

  // Use the event flags and, if necessary, lstat() result to determine the current and previous kinds of
  // this entry.
  void determine_entry_kinds()
  {
    if (flag_file && !flag_directory) {
      former_kind = current_kind = KIND_FILE;
      return;
    }

    if (flag_directory && !flag_file) {
      former_kind = current_kind = KIND_DIRECTORY;
      return;
    }

    // Flags are ambiguous. Try to check lstat().
    ensure_lstat();
    if (!is_present) {
      // Check the cache to see what this entry was the last time it produced an event.
      shared_ptr<CacheEntry> entry = cache.at_path(event_path);
      former_kind = entry ? entry->entry_kind : KIND_UNKNOWN;

      // Because both flags are set on the event, it must have changed from one to the other over the lifespan
      // of this entry.
      if (former_kind == KIND_FILE) current_kind = KIND_DIRECTORY;
      if (former_kind == KIND_DIRECTORY) current_kind = KIND_FILE;

      return;
    }

    // We know what the entry is now. Because both flags have been set on the event, it must have been different before.
    if ((path_stat.st_mode & S_IFREG) != 0) {
      former_kind = KIND_DIRECTORY;
      current_kind = KIND_FILE;
    } else if ((path_stat.st_mode & S_IFDIR) != 0) {
      former_kind = KIND_FILE;
      current_kind = KIND_DIRECTORY;
    }

    // Leave both as KIND_UNKNOWN.
  }

  // Check the recently-seen entry cache for this entry.
  void check_cache()
  {
    shared_ptr<CacheEntry> maybe = cache.at_path(event_path);

    seen_before = maybe && maybe->entry_kind == current_kind;
    was_different_entry = current_kind != former_kind && maybe;
  }

  // Emit messages for events that have unambiguous flags.
  bool emit_if_unambiguous()
  {
    ensure_lstat();

    if (flag_created && !(flag_deleted || flag_modified || flag_renamed)) {
      LOGGER << "Unambiguous creation." << endl;
      cache.does_exist(event_path, current_kind, path_stat.st_ino, path_stat.st_size);
      handler.enqueue_creation(event_path, current_kind);
      return true;
    }

    if (flag_deleted && !(flag_created || flag_modified || flag_renamed)) {
      LOGGER << "Unambiguous deletion." << endl;
      cache.does_not_exist(event_path);
      handler.enqueue_deletion(event_path, current_kind);
      return true;
    }

    if (flag_modified && !(flag_created || flag_deleted || flag_renamed)) {
      LOGGER << "Unambiguous modification." << endl;
      cache.does_exist(event_path, current_kind, path_stat.st_ino, path_stat.st_size);
      handler.enqueue_modification(event_path, current_kind);
      return true;
    }

    if (flag_renamed && !(flag_created || flag_deleted || flag_modified)) {
      LOGGER << "Unambiguous rename." << endl;

      if (is_present) {
        rename_buffer.observe_present_entry(event_path, current_kind, path_stat.st_ino, path_stat.st_size);
      } else {
        shared_ptr<CacheEntry> entry = cache.at_path(event_path);
        if (entry != nullptr) {
          rename_buffer.observe_absent_entry(event_path, current_kind, entry->inode, entry->size);
        } else {
          rename_buffer.observe_absent_entry(event_path, current_kind);
        }
      }

      return true;
    }

    return false;
  }

  // Emit messages based on the last observed state of this entry if it no longer exists.
  bool emit_if_absent()
  {
    ensure_lstat();
    if (is_present) return false;

    LOGGER << "Entry is no longer present." << endl;

    shared_ptr<CacheEntry> entry = cache.at_path(event_path);
    cache.does_not_exist(event_path);

    if (flag_renamed) {
      if (entry != nullptr) {
        rename_buffer.observe_absent_entry(event_path, current_kind, entry->inode, entry->size);
      } else {
        rename_buffer.observe_absent_entry(event_path, current_kind);
      }
      return true;
    }

    if (!seen_before) {
      if (was_different_entry) {
        // Entry was last seen as a directory, but the latest event has it flagged as a file (or vice versa).
        // The directory must have been deleted.
        handler.enqueue_deletion(event_path, former_kind);
      }

      // Entry has not been seen before, so we must have missed its creation event.
      handler.enqueue_creation(event_path, former_kind);
    }

    // It isn't there now, so it must have been deleted.
    handler.enqueue_deletion(event_path, current_kind);
    return true;
  }

  // Emit messages based on the event flags and the current lstat() output.
  bool emit_if_present()
  {
    ensure_lstat();
    if (!is_present) return false;

    LOGGER << "Entry is still present." << endl;

    cache.does_exist(event_path, current_kind, path_stat.st_ino, path_stat.st_size);

    if (flag_renamed) {
      rename_buffer.observe_present_entry(event_path, current_kind, path_stat.st_ino, path_stat.st_size);
      return true;
    }

    if (seen_before) {
      // This is *not* the first time an event at this path has been seen.
      if (flag_deleted) {
        // Rapid creation and deletion. There may be a lost modification event just before deletion or just after
        // recreation.
        handler.enqueue_deletion(event_path, former_kind);
        handler.enqueue_creation(event_path, current_kind);
      } else {
        // Modification of an existing entry.
        handler.enqueue_modification(event_path, current_kind);
      }
    } else {
      // This *is* the first time an event has been seen at this path.
      if (flag_deleted) {
        // The only way for the deletion flag to be set on an entry we haven't seen before is for the entry to
        // be rapidly created, deleted, and created again.
        handler.enqueue_creation(event_path, former_kind);
        handler.enqueue_deletion(event_path, former_kind);
        handler.enqueue_creation(event_path, current_kind);
      } else {
        // Otherwise, it must have been created. This may conceal a separate modification event just after
        // the entry's creation.
        handler.enqueue_creation(event_path, current_kind);
      }
    }

    return true;
  }

  // Call lstat() on the entry path if it has not already been called. After this function returns,
  // path_stat and is_present will be populated.
  void ensure_lstat()
  {
    if (stat_performed) return;

    if (lstat(event_path.c_str(), &path_stat) != 0) {
      errno_t stat_errno = errno;

      if (stat_errno == ENOENT) {
        is_present = false;
      }

      // Ignore lstat() errors on entries that:
      // (a) we aren't allowed to see
      // (b) are at paths with too many symlinks or looping symlinks
      // (c) have names that are too long
      // (d) have a path component that is (no longer) a directory
      // Log any other errno that we see.
      if (stat_errno != ENOENT &&
          stat_errno != EACCES &&
          stat_errno != ELOOP &&
          stat_errno != ENAMETOOLONG &&
          stat_errno != ENOTDIR) {
        LOGGER << "lstat(" << event_path << ") failed with errno " << errno << "." << endl;
      }
    } else {
      is_present = true;
    }

    stat_performed = true;
  }

  EventHandler &handler;
  RecentFileCache &cache;
  RenameBuffer &rename_buffer;

  string &event_path;
  FSEventStreamEventFlags flags;

  bool flag_created;
  bool flag_deleted;
  bool flag_modified;
  bool flag_renamed;
  bool flag_file;
  bool flag_directory;

  bool stat_performed;
  struct stat path_stat;
  bool is_present;

  EntryKind former_kind = KIND_UNKNOWN;
  EntryKind current_kind = KIND_UNKNOWN;

  bool seen_before;
  bool was_different_entry;
};

EventHandler::EventHandler(vector<Message> &messages, RecentFileCache &cache, ChannelID channel_id) :
  messages{messages},
  channel_id{channel_id},
  cache{cache},
  rename_buffer(this)
{
  //
}

void EventHandler::handle(string &event_path, FSEventStreamEventFlags flags)
{
  EventFunctor callable(*this, event_path, flags);
  callable();
}

void EventHandler::enqueue_creation(string event_path, const EntryKind &kind)
{
  string empty_path;

  FileSystemPayload payload(channel_id, ACTION_CREATED, kind, move(event_path), move(empty_path));
  Message event_message(move(payload));

  LOGGER << "Emitting filesystem message " << event_message << endl;

  messages.push_back(move(event_message));
}

void EventHandler::enqueue_modification(string event_path, const EntryKind &kind)
{
  string empty_path;

  FileSystemPayload payload(channel_id, ACTION_MODIFIED, kind, move(event_path), move(empty_path));
  Message event_message(move(payload));

  LOGGER << "Emitting filesystem message " << event_message << endl;

  messages.push_back(move(event_message));
}

void EventHandler::enqueue_deletion(string event_path, const EntryKind &kind)
{
  string empty_path;

  FileSystemPayload payload(channel_id, ACTION_DELETED, kind, move(event_path), move(empty_path));
  Message event_message(move(payload));

  LOGGER << "Emitting filesystem message " << event_message << endl;

  messages.push_back(move(event_message));
}

void EventHandler::enqueue_rename(string old_path, string new_path, const EntryKind &kind)
{
  FileSystemPayload payload(channel_id, ACTION_RENAMED, kind, move(old_path), move(new_path));
  Message event_message(move(payload));

  LOGGER << "Emitting filesystem message " << event_message << endl;

  messages.push_back(move(event_message));
}
