#ifndef FLAGS_H
#define FLAGS_H

#include <CoreServices/CoreServices.h>

const CFAbsoluteTime LATENCY = 0;

const FSEventStreamEventFlags CREATE_FLAGS = kFSEventStreamEventFlagItemCreated;

const FSEventStreamEventFlags DELETED_FLAGS = kFSEventStreamEventFlagItemRemoved;

const FSEventStreamEventFlags MODIFY_FLAGS = kFSEventStreamEventFlagItemInodeMetaMod
  | kFSEventStreamEventFlagItemFinderInfoMod | kFSEventStreamEventFlagItemChangeOwner
  | kFSEventStreamEventFlagItemXattrMod | kFSEventStreamEventFlagItemModified;

const FSEventStreamEventFlags RENAME_FLAGS = kFSEventStreamEventFlagItemRenamed;

const FSEventStreamEventFlags IS_FILE = kFSEventStreamEventFlagItemIsFile;

const FSEventStreamEventFlags IS_DIRECTORY = kFSEventStreamEventFlagItemIsDir;

const FSEventStreamEventFlags IS_SYMLINK = kFSEventStreamEventFlagItemIsSymlink;

const CFTimeInterval RENAME_TIMEOUT = 0.05;

#endif
