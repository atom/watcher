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
  FileSystemEvent(FileSystemEvent&& original);
  ~FileSystemEvent() {};

  std::string describe();
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
  CommandEvent(CommandEvent&& original);
  ~CommandEvent() {};

  const CommandAction& get_action();
  const std::string& get_root();

  std::string describe();
private:
  const CommandAction action;
  const std::string root;
};

class AckEvent {
public:
  AckEvent(const void *event);
  AckEvent(AckEvent&& original) = default;
  ~AckEvent() {};

  std::string describe();
private:
  const void* event;
};

enum EventKind {
  KIND_FILESYSTEM,
  KIND_COMMAND,
  KIND_ACK
};

class Event {
public:
  explicit Event(FileSystemEvent &&e);
  explicit Event(CommandEvent &&e);
  explicit Event(AckEvent &&e);
  Event(Event&& original);
  ~Event() {};

  FileSystemEvent *as_filesystem();
  CommandEvent *as_command();
  AckEvent *as_ack();

  std::string describe();
private:
  EventKind kind;
  union
  {
    FileSystemEvent fsEvent;
    CommandEvent commandEvent;
    AckEvent ackEvent;
  };
};

std::ostream& operator<<(std::ostream& stream, const FileSystemEvent& e);
std::ostream& operator<<(std::ostream& stream, const CommandEvent& e);
std::ostream& operator<<(std::ostream& stream, const AckEvent& e);
std::ostream& operator<<(std::ostream& stream, const Event& e);

#endif
