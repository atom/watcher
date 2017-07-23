#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <iostream>
#include <memory>
#include <cstdint>

enum FileSystemAction {
  ACTION_CREATED = 0,
  ACTION_DELETED = 1,
  ACTION_MODIFIED = 2,
  ACTION_RENAMED = 3
};

std::ostream &operator<<(std::ostream &out, FileSystemAction action);

enum EntryKind {
  KIND_FILE = 0,
  KIND_DIRECTORY = 1,
  KIND_UNKNOWN = 2
};

std::ostream &operator<<(std::ostream &out, EntryKind kind);

typedef uint_fast32_t ChannelID;

static const ChannelID NULL_CHANNEL_ID = 0;

class FileSystemPayload {
public:
  FileSystemPayload(
    const ChannelID channel_id,
    const FileSystemAction action,
    const EntryKind entry_kind,
    const std::string &&old_path,
    const std::string &&new_path
  );
  FileSystemPayload(FileSystemPayload &&original);
  ~FileSystemPayload() {};

  const ChannelID &get_channel_id() const;
  const FileSystemAction &get_filesystem_action() const;
  const EntryKind &get_entry_kind() const;
  const std::string &get_old_path() const;
  const std::string &get_new_path() const;

  std::string describe() const;
private:
  const ChannelID channel_id;
  const FileSystemAction action;
  const EntryKind entry_kind;
  const std::string old_path;
  const std::string new_path;
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
  AckPayload(const CommandID key, const ChannelID channel_id, bool success, const std::string &&message);
  AckPayload(AckPayload &&original) = default;
  ~AckPayload() {};

  const CommandID &get_key() const;
  const ChannelID &get_channel_id() const;
  const bool &was_successful() const;
  const std::string &get_message() const;

  std::string describe() const;
private:
  const CommandID key;
  const ChannelID channel_id;
  const bool success;
  const std::string message;
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
