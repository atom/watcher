#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "result.h"

enum FileSystemAction
{
  ACTION_CREATED = 0,
  ACTION_DELETED = 1,
  ACTION_MODIFIED = 2,
  ACTION_RENAMED = 3
};

std::ostream &operator<<(std::ostream &out, FileSystemAction action);

enum EntryKind
{
  KIND_FILE = 0,
  KIND_DIRECTORY = 1,
  KIND_UNKNOWN = 2
};

std::ostream &operator<<(std::ostream &out, EntryKind kind);

bool kinds_are_different(EntryKind a, EntryKind b);

using Entry = std::pair<std::string, EntryKind>;

using ChannelID = uint_fast32_t;

const ChannelID NULL_CHANNEL_ID = 0;

class FileSystemPayload
{
public:
  FileSystemPayload(ChannelID channel_id,
    FileSystemAction action,
    EntryKind entry_kind,
    std::string &&old_path,
    std::string &&path);
  FileSystemPayload(FileSystemPayload &&original) noexcept;
  ~FileSystemPayload() = default;

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
  std::string old_path;
  std::string path;
};

enum CommandAction
{
  COMMAND_ADD,
  COMMAND_REMOVE,
  COMMAND_LOG_FILE,
  COMMAND_LOG_STDERR,
  COMMAND_LOG_STDOUT,
  COMMAND_LOG_DISABLE,
  COMMAND_POLLING_INTERVAL,
  COMMAND_POLLING_THROTTLE,
  COMMAND_DRAIN,
  COMMAND_MIN = COMMAND_ADD,
  COMMAND_MAX = COMMAND_DRAIN
};

using CommandID = uint_fast32_t;

const CommandID NULL_COMMAND_ID = 0;

class CommandPayload
{
public:
  CommandPayload(CommandAction action,
    CommandID id = NULL_COMMAND_ID,
    std::string &&root = "",
    uint_fast32_t arg = NULL_CHANNEL_ID);
  CommandPayload(CommandPayload &&original) noexcept;
  ~CommandPayload() = default;

  CommandPayload(const CommandPayload &original) = delete;
  CommandPayload &operator=(const CommandPayload &original) = delete;
  CommandPayload &operator=(CommandPayload &&original) = delete;

  CommandID get_id() const { return id; }
  const CommandAction &get_action() const { return action; }
  const std::string &get_root() const { return root; }
  const uint_fast32_t &get_arg() const { return arg; }
  const ChannelID &get_channel_id() const { return arg; }

  std::string describe() const;

private:
  const CommandID id;
  const CommandAction action;
  std::string root;
  const uint_fast32_t arg;
};

class AckPayload
{
public:
  AckPayload(CommandID key, ChannelID channel_id, bool success, std::string &&message);
  AckPayload(AckPayload &&original) = default;
  ~AckPayload() = default;

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
  std::string message;
};

enum MessageKind
{
  KIND_FILESYSTEM,
  KIND_COMMAND,
  KIND_ACK
};

class Message
{
public:
  static Message ack(const Message &original, bool success, std::string &&message = "");

  static Message ack(const Message &original, const Result<> &result);

  explicit Message(FileSystemPayload &&payload);
  explicit Message(CommandPayload &&payload);
  explicit Message(AckPayload &&payload);
  Message(Message &&original) noexcept;
  ~Message();

  Message(const Message &) = delete;
  Message &operator=(const Message &) = delete;
  Message &operator=(Message &&) = delete;

  const FileSystemPayload *as_filesystem() const;
  const CommandPayload *as_command() const;
  const AckPayload *as_ack() const;

  std::string describe() const;

private:
  MessageKind kind;
  union
  {
    FileSystemPayload filesystem_payload;
    CommandPayload command_payload;
    AckPayload ack_payload;
    bool pending{false};
  };
};

std::ostream &operator<<(std::ostream &stream, const FileSystemPayload &e);
std::ostream &operator<<(std::ostream &stream, const CommandPayload &e);
std::ostream &operator<<(std::ostream &stream, const AckPayload &e);
std::ostream &operator<<(std::ostream &stream, const Message &e);

#endif
