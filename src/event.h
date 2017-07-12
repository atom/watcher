#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <memory>

enum FileSystemAction {
  ACTION_CREATED = 0,
  ACTION_DELETED = 1,
  ACTION_MODIFIED = 2,
  ACTION_RENAMED = 3
};

enum EntryKind {
  KIND_FILE = 0,
  KIND_DIRECTORY = 1
};

class FileSystemEvent {
public:
  FileSystemEvent(
    const FileSystemAction action,
    const EntryKind entryKind,
    const std::string &&dirname,
    const std::string &&oldBaseName,
    const std::string &&newBaseName
  );
  ~FileSystemEvent();

private:
  const FileSystemAction action;
  const EntryKind entryKind;
  const std::string dirname;
  const std::string oldBaseName;
  const std::string newBaseName;
};

enum CommandAction {
  COMMAND_ADD,
  COMMAND_REMOVE,
  COMMAND_LOG_FILE,
  COMMAND_LOG_DISABLE
};

class CommandEvent {
public:
  CommandEvent(const CommandAction action, const std::string &&root);

private:
  const CommandAction action;
  const std::string root;
};

class AckEvent {
public:
  AckEvent(const void *event);
  ~AckEvent();

private:
  const void* event;
};

enum EventKind {
  FILESYSTEM,
  COMMAND,
  ACK
};

class Event {
public:
  Event();
  ~Event();

private:
  EventKind kind;
  union
  {
    FileSystemEvent fsEvent;
    CommandEvent commandEvent;
    AckEvent ackEvent;
  };
};

#endif
