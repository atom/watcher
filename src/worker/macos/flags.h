#ifndef FLAGS_H
#define FLAGS_H

#include <CoreServices/CoreServices.h>

static const CFAbsoluteTime LATENCY = 0;

static const FSEventStreamEventFlags CREATE_FLAGS = kFSEventStreamEventFlagItemCreated;

static const FSEventStreamEventFlags DELETED_FLAGS = kFSEventStreamEventFlagItemRemoved;

static const FSEventStreamEventFlags MODIFY_FLAGS = kFSEventStreamEventFlagItemInodeMetaMod
  | kFSEventStreamEventFlagItemFinderInfoMod | kFSEventStreamEventFlagItemChangeOwner
  | kFSEventStreamEventFlagItemXattrMod | kFSEventStreamEventFlagItemModified;

static const FSEventStreamEventFlags RENAME_FLAGS = kFSEventStreamEventFlagItemRenamed;

static const FSEventStreamEventFlags IS_FILE = kFSEventStreamEventFlagItemIsFile;

static const FSEventStreamEventFlags IS_DIRECTORY = kFSEventStreamEventFlagItemIsDir;

#endif
