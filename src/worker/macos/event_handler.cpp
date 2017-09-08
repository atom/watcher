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
    flags{flags}
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
    collect_info();

    LOGGER << "Former: " << *former << " Current: " << *current << endl;

    if (emit_if_unambiguous()) return;
    if (emit_if_rename()) return;
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

  // Check the recently-seen entry cache for this entry.
  void collect_info()
  {
    former = cache.at_path(event_path, flag_file, flag_directory);
    current = StatResult::at(event_path, flag_file, flag_directory);

    cache.insert(current);
  }

  // Emit messages for events that have unambiguous flags.
  bool emit_if_unambiguous()
  {
    if (flag_created && !(flag_deleted || flag_modified || flag_renamed)) {
      LOGGER << "Unambiguous creation." << endl;
      handler.enqueue_creation(event_path, current->get_entry_kind());
      return true;
    }

    if (flag_deleted && !(flag_created || flag_modified || flag_renamed)) {
      LOGGER << "Unambiguous deletion." << endl;

      EntryKind former_kind = KIND_UNKNOWN;
      if (current->get_entry_kind() != KIND_UNKNOWN) {
        former_kind = current->get_entry_kind();
      } else if (former->get_entry_kind() != KIND_UNKNOWN) {
        former_kind = former->get_entry_kind();
      }

      handler.enqueue_deletion(event_path, former_kind);
      return true;
    }

    if (flag_modified && !(flag_created || flag_deleted || flag_renamed)) {
      LOGGER << "Unambiguous modification." << endl;
      handler.enqueue_modification(event_path, current->get_entry_kind());
      return true;
    }

    return false;
  }

  // Present the current and former entries to the RenameBuffer if the rename flag is set. This may
  // emit a rename event if a matching entry pair is discovered, or it will buffer the entries to find
  // matches when they arrive.
  bool emit_if_rename()
  {
    if (flag_renamed) {
      rename_buffer.observe_entry(former, current);
      return true;
    }

    return false;
  }

  // Emit messages based on the last observed state of this entry if it no longer exists.
  bool emit_if_absent()
  {
    if (current->is_present()) return false;

    LOGGER << "Entry is no longer present." << endl;

    if (former->is_present() && kinds_are_different(former->get_entry_kind(), current->get_entry_kind())) {
      // Entry was last seen as a directory, but the latest event has it flagged as a file (or vice versa).
      // The directory must have been deleted.
      handler.enqueue_deletion(former->get_path(), former->get_entry_kind());
      handler.enqueue_creation(current->get_path(), current->get_entry_kind());
    } else {
      // Entry has not been seen before, so we must have missed its creation event.
      handler.enqueue_creation(current->get_path(), current->get_entry_kind());
    }

    // It isn't there now, so it must have been deleted.
    handler.enqueue_deletion(current->get_path(), current->get_entry_kind());
    return true;
  }

  // Emit messages based on the event flags and the current lstat() output.
  bool emit_if_present()
  {
    if (current->is_absent()) return false;

    LOGGER << "Entry is still present." << endl;

    if (former->is_present()) {
      // This is *not* the first time an event at this path has been seen.
      if (flag_deleted) {
        // Rapid creation and deletion. There may be a lost modification event just before deletion or just after
        // recreation.
        handler.enqueue_deletion(former->get_path(), former->get_entry_kind());
        handler.enqueue_creation(current->get_path(), current->get_entry_kind());
      } else {
        // Modification of an existing entry.
        handler.enqueue_modification(current->get_path(), current->get_entry_kind());
      }
    } else {
      // This *is* the first time an event has been seen at this path.
      if (flag_deleted) {
        // The only way for the deletion flag to be set on an entry we haven't seen before is for the entry to
        // be rapidly created, deleted, and created again.
        handler.enqueue_creation(former->get_path(), former->get_entry_kind());
        handler.enqueue_deletion(former->get_path(), former->get_entry_kind());
        handler.enqueue_creation(current->get_path(), current->get_entry_kind());
      } else {
        // Otherwise, it must have been created. This may conceal a separate modification event just after
        // the entry's creation.
        handler.enqueue_creation(current->get_path(), current->get_entry_kind());
      }
    }

    return true;
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

  shared_ptr<StatResult> former;
  shared_ptr<StatResult> current;
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

void EventHandler::flush()
{
  rename_buffer.flush_unmatched();
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
