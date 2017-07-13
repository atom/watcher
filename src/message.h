#ifndef MESSAGE_H
#define MESSAGE_H

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

class FileSystemPayload {
public:
  FileSystemPayload(
    const FileSystemAction action,
    const EntryKind entryKind,
    const std::string &&dirname,
    const std::string &&oldBaseName,
    const std::string &&newBaseName
  );
  FileSystemPayload(FileSystemPayload&& original);
  ~FileSystemPayload() {};

  std::string describe() const;
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

class CommandPayload {
public:
  CommandPayload(const CommandAction action, const std::string &&root);
  CommandPayload(CommandPayload&& original);
  ~CommandPayload() {};

  const CommandAction& get_action() const;
  const std::string& get_root() const;

  std::string describe() const;
private:
  const CommandAction action;
  const std::string root;
};

class AckPayload {
public:
  AckPayload(const void *event);
  AckPayload(AckPayload&& original) = default;
  ~AckPayload() {};

  std::string describe() const;
private:
  const void* event;
};

enum MessageKind {
  KIND_FILESYSTEM,
  KIND_COMMAND,
  KIND_ACK
};

class Message {
public:
  explicit Message(FileSystemPayload &&e);
  explicit Message(CommandPayload &&e);
  explicit Message(AckPayload &&e);
  Message(Message&& original);
  ~Message();

  const FileSystemPayload* as_filesystem() const;
  const CommandPayload* as_command() const;
  const AckPayload* as_ack() const;

  std::string describe() const;
private:
  MessageKind kind;
  union
  {
    FileSystemPayload fileSystemPayload;
    CommandPayload commandPayload;
    AckPayload ackPayload;
    bool pending;
  };
};

std::ostream& operator<<(std::ostream& stream, const FileSystemPayload& e);
std::ostream& operator<<(std::ostream& stream, const CommandPayload& e);
std::ostream& operator<<(std::ostream& stream, const AckPayload& e);
std::ostream& operator<<(std::ostream& stream, const Message& e);

#endif
