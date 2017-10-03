#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <utility>
#include <iostream>
#include <memory>
#include <cstdint>

#include "result.h"

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

bool kinds_are_different(EntryKind a, EntryKind b);

typedef std::pair<std::string, EntryKind> Entry;

typedef uint_fast32_t ChannelID;

const ChannelID NULL_CHANNEL_ID = 0;

class FileSystemPayload {
public:
  FileSystemPayload(
    const ChannelID channel_id,
    const FileSystemAction action,
    const EntryKind entry_kind,
    const std::string &&old_path,
    const std::string &&path
  );
  FileSystemPayload(FileSystemPayload &&original);
  ~FileSystemPayload() {};

  FileSystemPayload(const FileSystemPayload &original) = delete;
  FileSystemPayload &operator=(const FileSystemPayload &original) = delete;
  FileSystemPayload &operator=(FileSystemPayload &&original) = delete;

  const ChannelID &get_channel_id() const { return channel_id; }
  const FileSystemAction &get_filesystem_action() const { return action; }
  const EntryKind &get_entry_kind() const { return entry_kind; }
  const std::string &get_old_path() const { return old_path; }
  const std::string &get_path() const { return path; }

  std::string describe() const;
private:
  const ChannelID channel_id;
  const FileSystemAction action;
  const EntryKind entry_kind;
  const std::string old_path;
  const std::string path;
};

enum CommandAction {
  COMMAND_ADD,
  COMMAND_REMOVE,
  COMMAND_LOG_FILE,
  COMMAND_LOG_STDERR,
  COMMAND_LOG_STDOUT,
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

  CommandPayload(const CommandPayload &original) = delete;
  CommandPayload &operator=(const CommandPayload &original) = delete;
  CommandPayload &operator=(CommandPayload &&original) = delete;

  CommandID get_id() const { return id; }
  const CommandAction &get_action() const { return action; }
  const std::string &get_root() const { return root; }
  const ChannelID &get_channel_id() const { return channel_id; }

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

  AckPayload(const AckPayload &original) = delete;
  AckPayload &operator=(const AckPayload &original) = delete;
  AckPayload &operator=(AckPayload &&original) = delete;

  const CommandID &get_key() const { return key; }
  const ChannelID &get_channel_id() const { return channel_id; }
  const bool &was_successful() const { return success; }
  const std::string &get_message() const { return message; }

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
  static Message ack(const Message &original, bool success, const std::string &&message = "");

  static Message ack(const Message &original, const Result<> &result);

  explicit Message(FileSystemPayload &&e);
  explicit Message(CommandPayload &&e);
  explicit Message(AckPayload &&e);
  Message(Message&& original);
  ~Message();

  Message(const Message&) = delete;
  Message &operator=(const Message&) = delete;
  Message &operator=(Message&&) = delete;

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
