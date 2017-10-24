#include "batch_handler.h"

#include <CoreServices/CoreServices.h>
#include <cerrno>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <utility>

#include "../../log.h"
#include "../../message.h"
#include "flags.h"
#include "recent_file_cache.h"
#include "rename_buffer.h"

using std::dec;
using std::endl;
using std::hex;
using std::move;
using std::ostream;
using std::shared_ptr;
using std::string;

BatchHandler::BatchHandler(ChannelMessageBuffer &message_buffer,
  RecentFileCache &cache,
  RenameBuffer &rename_buffer,
  bool recursive,
  const string &root_path) :
  cache{cache},
  message_buffer{message_buffer},
  rename_buffer{rename_buffer},
  recursive{recursive},
  root_path{root_path}
{
  //
}

void BatchHandler::event(string &&event_path, FSEventStreamEventFlags flags)
{
  Event event(*this, move(event_path), flags);

  if (event.skip_recursive_event()) return;
  event.collect_info();
  event.report();

  if (event.emit_if_unambiguous()) return;
  if (event.emit_if_rename()) return;
  if (event.emit_if_absent()) return;
  event.emit_if_present();
}

Event::Event(BatchHandler &batch, std::string &&event_path, FSEventStreamEventFlags flags) :
  handler{batch},
  event_path{move(event_path)},
  flags{flags},
  former{nullptr},
  current{nullptr}
{
  //
}

bool Event::skip_recursive_event()
{
  if (is_recursive()) return false;
  if (event_path == root_path()) return false;

  char parent_dir[PATH_MAX];
  char *result = dirname_r(event_path.c_str(), parent_dir);
  if (result == nullptr) return false;
  if (root_path() == parent_dir) return false;

  return true;
}

void Event::collect_info()
{
  former = cache().former_at_path(event_path, flag_file(), flag_directory());
  current = cache().current_at_path(event_path, flag_file(), flag_directory());
}

void Event::report()
{
  ostream &logline = LOGGER;
  logline << "Event at [" << event_path << "] flags " << hex << flags << dec << " [";

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

  logline << " ] former=" << former->to_string(false) << " current=" << current->to_string(false) << endl;
}

bool Event::emit_if_unambiguous()
{
  if (flag_created() && !(flag_deleted() || flag_modified() || flag_renamed())) {
    message_buffer().created(move(event_path), current->get_entry_kind());
    return true;
  }

  if (flag_deleted() && !(flag_created() || flag_modified() || flag_renamed())) {
    EntryKind former_kind = KIND_UNKNOWN;
    if (current->get_entry_kind() != KIND_UNKNOWN) {
      former_kind = current->get_entry_kind();
    } else if (former->get_entry_kind() != KIND_UNKNOWN) {
      former_kind = former->get_entry_kind();
    }

    message_buffer().deleted(move(event_path), former_kind);
    return true;
  }

  if (flag_modified() && !(flag_created() || flag_deleted() || flag_renamed())) {
    message_buffer().modified(move(event_path), current->get_entry_kind());
    return true;
  }

  return false;
}

bool Event::emit_if_rename()
{
  if (flag_renamed()) {
    rename_buffer().observe_event(*this);
    return true;
  }

  return false;
}

bool Event::emit_if_absent()
{
  if (current->is_present()) return false;

  if (former->is_present() && kinds_are_different(former->get_entry_kind(), current->get_entry_kind()) && flag_deleted()
    && flag_created()) {
    // Entry was last seen as a directory, but the latest event has it flagged as a file (or vice versa).
    // The directory must have been deleted.
    message_buffer().deleted(string(former->get_path()), former->get_entry_kind());
    message_buffer().created(string(current->get_path()), current->get_entry_kind());
  } else if (former->is_absent() && flag_created()) {
    // Entry has not been seen before, so this must be its creation event.
    message_buffer().created(string(current->get_path()), current->get_entry_kind());
  }

  // It isn't there now, so it must have been deleted.
  if (flag_deleted()) {
    message_buffer().deleted(string(current->get_path()), current->get_entry_kind());
  }
  return true;
}

bool Event::emit_if_present()
{
  if (current->is_absent()) return false;

  if (former->is_present()) {
    // This is *not* the first time an event at this path has been seen.
    if (flag_deleted() && flag_created()) {
      // Rapid creation and deletion. There may be a lost modification event just before deletion or just after
      // recreation.
      message_buffer().deleted(string(former->get_path()), former->get_entry_kind());
      message_buffer().created(string(current->get_path()), current->get_entry_kind());
    } else if (flag_modified()) {
      // Modification of an existing entry.
      message_buffer().modified(string(current->get_path()), current->get_entry_kind());
    }
  } else {
    // This *is* the first time an event has been seen at this path.
    if (flag_deleted() && flag_created()) {
      // The only way for the deletion flag to be set on an entry we haven't seen before is for the entry to
      // be rapidly created, deleted, and created again.
      message_buffer().created(string(former->get_path()), former->get_entry_kind());
      message_buffer().deleted(string(former->get_path()), former->get_entry_kind());
      message_buffer().created(string(current->get_path()), current->get_entry_kind());
    } else if (flag_created()) {
      // Otherwise, it must have been created. This may conceal a separate modification event just after
      // the entry's creation.
      message_buffer().created(string(current->get_path()), current->get_entry_kind());
    }
  }

  return true;
}
