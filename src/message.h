#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "result.h"
#include "status.h"

enum EntryKind
{
  KIND_FILE = 0,
  KIND_DIRECTORY = 1,
  KIND_SYMLINK = 2,
  KIND_UNKNOWN = 3,
  KIND_MIN = KIND_FILE,
  KIND_MAX = KIND_UNKNOWN
};

std::ostream &operator<<(std::ostream &out, EntryKind kind);

bool kinds_are_different(EntryKind a, EntryKind b);

using Entry = std::pair<std::string, EntryKind>;

using ChannelID = uint_fast32_t;

const ChannelID NULL_CHANNEL_ID = 0;

using RequestID = uint_fast32_t;

const RequestID NULL_REQUEST_ID = 0;

enum FileSystemAction
{
  ACTION_CREATED = 0,
  ACTION_DELETED = 1,
  ACTION_MODIFIED = 2,
  ACTION_RENAMED = 3,
  ACTION_MIN = ACTION_CREATED,
  ACTION_MAX = ACTION_RENAMED
};

std::ostream &operator<<(std::ostream &out, FileSystemAction action);

class FileSystemPayload
{
public:
  static FileSystemPayload created(ChannelID channel_id, std::string &&path, const EntryKind &kind)
  {
    return FileSystemPayload(channel_id, ACTION_CREATED, kind, "", std::move(path));
  }

  static FileSystemPayload modified(ChannelID channel_id, std::string &&path, const EntryKind &kind)
  {
    return FileSystemPayload(channel_id, ACTION_MODIFIED, kind, "", std::move(path));
  }

  static FileSystemPayload deleted(ChannelID channel_id, std::string &&path, const EntryKind &kind)
  {
    return FileSystemPayload(channel_id, ACTION_DELETED, kind, "", std::move(path));
  }

  static FileSystemPayload renamed(ChannelID channel_id,
    std::string &&old_path,
    std::string &&path,
    const EntryKind &kind)
  {
    return FileSystemPayload(channel_id, ACTION_RENAMED, kind, std::move(old_path), std::move(path));
  }

  FileSystemPayload(FileSystemPayload &&original) noexcept;

  ~FileSystemPayload() = default;

  const ChannelID &get_channel_id() const { return channel_id; }

  const FileSystemAction &get_filesystem_action() const { return action; }

  const EntryKind &get_entry_kind() const { return entry_kind; }

  const std::string &get_old_path() const { return old_path; }

  const std::string &get_path() const { return path; }

  std::string describe() const;

  FileSystemPayload(const FileSystemPayload &original) = delete;
  FileSystemPayload &operator=(const FileSystemPayload &original) = delete;
  FileSystemPayload &operator=(FileSystemPayload &&original) = delete;

private:
  FileSystemPayload(ChannelID channel_id,
    FileSystemAction action,
    EntryKind entry_kind,
    std::string &&old_path,
    std::string &&path);

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
  COMMAND_CACHE_SIZE,
  COMMAND_DRAIN,
  COMMAND_STATUS,
  COMMAND_MIN = COMMAND_ADD,
  COMMAND_MAX = COMMAND_STATUS
};

using CommandID = uint_fast32_t;

const CommandID NULL_COMMAND_ID = 0;

class CommandPayload
{
public:
  CommandPayload(CommandPayload &&original) noexcept;

  explicit CommandPayload(const CommandPayload &original) = default;

  ~CommandPayload() = default;

  CommandID get_id() const { return id; }

  const CommandAction &get_action() const { return action; }

  const std::string &get_root() const { return root; }

  const uint_fast32_t &get_arg() const { return arg; }

  const ChannelID &get_channel_id() const { return arg; }

  const RequestID &get_request_id() const { return arg; }

  const bool &get_recursive() const { return recursive; }

  const size_t &get_split_count() const { return split_count; }

  std::string describe() const;

  CommandPayload &operator=(const CommandPayload &original) = delete;
  CommandPayload &operator=(CommandPayload &&original) = delete;

private:
  CommandPayload(CommandAction action,
    CommandID id,
    std::string &&root,
    uint_fast32_t arg,
    bool recursive,
    size_t split_count);

  const CommandID id;
  const CommandAction action;
  std::string root;
  const uint_fast32_t arg;
  bool recursive;
  const size_t split_count;

  friend class CommandPayloadBuilder;
};

class CommandPayloadBuilder
{
public:
  static CommandPayloadBuilder add(ChannelID channel_id, std::string &&root, bool recursive, size_t split_count)
  {
    return CommandPayloadBuilder(COMMAND_ADD, std::move(root), channel_id, recursive, split_count);
  }

  static CommandPayloadBuilder remove(ChannelID channel_id)
  {
    return CommandPayloadBuilder(COMMAND_REMOVE, "", channel_id, false, 1);
  }

  static CommandPayloadBuilder log_to_file(std::string &&log_file)
  {
    return CommandPayloadBuilder(COMMAND_LOG_FILE, std::move(log_file), NULL_CHANNEL_ID, false, 1);
  }

  static CommandPayloadBuilder log_to_stderr()
  {
    return CommandPayloadBuilder(COMMAND_LOG_STDERR, "", NULL_CHANNEL_ID, false, 1);
  }

  static CommandPayloadBuilder log_to_stdout()
  {
    return CommandPayloadBuilder(COMMAND_LOG_STDOUT, "", NULL_CHANNEL_ID, false, 1);
  }

  static CommandPayloadBuilder log_disable()
  {
    return CommandPayloadBuilder(COMMAND_LOG_DISABLE, "", NULL_CHANNEL_ID, false, 1);
  }

  static CommandPayloadBuilder polling_interval(const uint_fast32_t &interval)
  {
    return CommandPayloadBuilder(COMMAND_POLLING_INTERVAL, "", interval, false, 1);
  }

  static CommandPayloadBuilder polling_throttle(const uint_fast32_t &throttle)
  {
    return CommandPayloadBuilder(COMMAND_POLLING_THROTTLE, "", throttle, false, 1);
  }

  static CommandPayloadBuilder cache_size(uint_fast32_t maximum_size)
  {
    return CommandPayloadBuilder(COMMAND_CACHE_SIZE, "", maximum_size, false, 1);
  }

  static CommandPayloadBuilder drain() { return CommandPayloadBuilder(COMMAND_DRAIN, "", NULL_CHANNEL_ID, false, 1); }

  static CommandPayloadBuilder status(RequestID request_id)
  {
    return CommandPayloadBuilder(COMMAND_STATUS, "", request_id, false, 1);
  }

  CommandPayloadBuilder(CommandPayloadBuilder &&original) noexcept :
    id{original.id},
    action{original.action},
    root{std::move(original.root)},
    arg{original.arg},
    recursive{original.recursive},
    split_count{original.split_count}
  {
    //
  }

  ~CommandPayloadBuilder() = default;

  CommandPayloadBuilder &set_id(CommandID id)
  {
    this->id = id;
    return *this;
  }

  CommandPayload build()
  {
    assert(action >= COMMAND_MIN && action <= COMMAND_MAX);
    return CommandPayload(action, id, std::move(root), arg, recursive, split_count);
  }

  CommandPayloadBuilder(const CommandPayloadBuilder &) = delete;
  CommandPayloadBuilder &operator=(const CommandPayloadBuilder &) = delete;
  CommandPayloadBuilder &operator=(CommandPayloadBuilder &&) = delete;

private:
  CommandPayloadBuilder(CommandAction action,
    std::string &&root,
    uint_fast32_t arg,
    bool recursive,
    size_t split_count) :
    id{NULL_COMMAND_ID},
    action{action},
    root{std::move(root)},
    arg{arg},
    recursive{recursive},
    split_count{split_count}
  {}

  CommandID id;
  CommandAction action;
  std::string root;
  uint_fast32_t arg;
  bool recursive;
  size_t split_count;
};

class AckPayload
{
public:
  AckPayload(CommandID key, ChannelID channel_id, bool success, std::string &&message);

  AckPayload(AckPayload &&original) = default;

  ~AckPayload() = default;

  const CommandID &get_key() const { return key; }

  const ChannelID &get_channel_id() const { return channel_id; }

  const bool &was_successful() const { return success; }

  const std::string &get_message() const { return message; }

  std::string describe() const;

  AckPayload(const AckPayload &original) = delete;
  AckPayload &operator=(const AckPayload &original) = delete;
  AckPayload &operator=(AckPayload &&original) = delete;

private:
  const CommandID key;
  const ChannelID channel_id;
  const bool success;
  std::string message;
};

class ErrorPayload
{
public:
  ErrorPayload(ChannelID channel_id, std::string &&message, bool fatal);

  ErrorPayload(ErrorPayload &&original) noexcept = default;

  ~ErrorPayload() = default;

  const ChannelID &get_channel_id() const { return channel_id; }

  const std::string &get_message() const { return message; }

  const bool &was_fatal() const { return fatal; }

  std::string describe() const;

  ErrorPayload(const ErrorPayload &) = delete;
  ErrorPayload &operator=(const ErrorPayload &) = delete;
  ErrorPayload &operator=(ErrorPayload &&) = delete;

private:
  const ChannelID channel_id;
  std::string message;
  const bool fatal;
};

class StatusPayload
{
public:
  StatusPayload(RequestID request_id, std::unique_ptr<Status> &&status);

  StatusPayload(StatusPayload &&original) noexcept = default;

  ~StatusPayload() = default;

  const RequestID &get_request_id() const { return request_id; }

  const Status &get_status() const { return *status; }

  std::string describe() const;

  StatusPayload(const StatusPayload &) = delete;
  StatusPayload &operator=(const StatusPayload &) = delete;
  StatusPayload &operator=(StatusPayload &&) = delete;

private:
  const RequestID request_id;
  std::unique_ptr<Status> status;
};

enum MessageKind
{
  MSG_FILESYSTEM,
  MSG_COMMAND,
  MSG_ACK,
  MSG_ERROR,
  MSG_STATUS,
  MSG_MIN = MSG_FILESYSTEM,
  MSG_MAX = MSG_STATUS
};

class Message
{
public:
  static Message ack(const Message &original, bool success, std::string &&message = "");

  static Message ack(const Message &original, const Result<> &result);

  explicit Message(FileSystemPayload &&payload);

  explicit Message(CommandPayload &&payload);

  explicit Message(AckPayload &&payload);

  explicit Message(ErrorPayload &&payload);

  explicit Message(StatusPayload &&payload);

  Message(Message &&original) noexcept;

  ~Message();

  const FileSystemPayload *as_filesystem() const;

  const CommandPayload *as_command() const;

  const AckPayload *as_ack() const;

  const ErrorPayload *as_error() const;

  const StatusPayload *as_status() const;

  std::string describe() const;

  Message(const Message &) = delete;
  Message &operator=(const Message &) = delete;
  Message &operator=(Message &&) = delete;

private:
  MessageKind kind;
  union
  {
    FileSystemPayload filesystem_payload;
    CommandPayload command_payload;
    AckPayload ack_payload;
    ErrorPayload error_payload;
    StatusPayload status_payload;
    bool pending{false};
  };
};

std::ostream &operator<<(std::ostream &stream, const FileSystemPayload &e);

std::ostream &operator<<(std::ostream &stream, const CommandPayload &e);

std::ostream &operator<<(std::ostream &stream, const AckPayload &e);

std::ostream &operator<<(std::ostream &stream, const ErrorPayload &e);

std::ostream &operator<<(std::ostream &stream, const StatusPayload &e);

std::ostream &operator<<(std::ostream &stream, const Message &e);

#endif
