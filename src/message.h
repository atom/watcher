#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <memory>
#include <cstdint>

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

typedef uint_fast32_t ChannelID;

static const ChannelID NULL_CHANNEL_ID = 0;

class FileSystemPayload {
public:
  FileSystemPayload(
    const ChannelID channel_id,
    const FileSystemAction action,
    const EntryKind entry_kind,
    const std::string &&dir_name,
    const std::string &&old_base_name,
    const std::string &&new_base_name
  );
  FileSystemPayload(FileSystemPayload &&original);
  ~FileSystemPayload() {};

  std::string describe() const;
private:
  const ChannelID channel_id;
  const FileSystemAction action;
  const EntryKind entry_kind;
  const std::string dirname;
  const std::string old_base_name;
  const std::string new_base_name;
};

enum CommandAction {
  COMMAND_ADD,
  COMMAND_REMOVE,
  COMMAND_LOG_FILE,
  COMMAND_LOG_DISABLE
};

typedef uint_fast32_t CommandID;

class CommandPayload {
public:
  CommandPayload(
    const CommandID id,
    const CommandAction action,
    const std::string &&root,
    const ChannelID channel_id = NULL_CHANNEL_ID
  );
  CommandPayload(CommandPayload &&original);
  ~CommandPayload() {};

  CommandID get_id() const;
  const CommandAction &get_action() const;
  const std::string &get_root() const;
  const ChannelID &get_channel_id() const;

  std::string describe() const;
private:
  const CommandID id;
  const CommandAction action;
  const std::string root;
  const ChannelID channel_id;
};

class AckPayload {
public:
  AckPayload(const CommandID key);
  AckPayload(AckPayload &&original) = default;
  ~AckPayload() {};

  CommandID get_key() const;

  std::string describe() const;
private:
  const CommandID key;
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
    FileSystemPayload filesystem_payload;
    CommandPayload command_payload;
    AckPayload ack_payload;
    bool pending;
  };
};

std::ostream& operator<<(std::ostream& stream, const FileSystemPayload& e);
std::ostream& operator<<(std::ostream& stream, const CommandPayload& e);
std::ostream& operator<<(std::ostream& stream, const AckPayload& e);
std::ostream& operator<<(std::ostream& stream, const Message& e);

#endif
