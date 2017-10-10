#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "message.h"

using std::move;
using std::ostream;
using std::ostringstream;
using std::string;

ostream &operator<<(ostream &out, FileSystemAction action)
{
  switch (action) {
    case ACTION_CREATED: out << "created"; break;
    case ACTION_DELETED: out << "deleted"; break;
    case ACTION_MODIFIED: out << "modified"; break;
    case ACTION_RENAMED: out << "renamed"; break;
    default: out << "!! FileSystemAction=" << static_cast<int>(action);
  }
  return out;
}

ostream &operator<<(ostream &out, EntryKind kind)
{
  switch (kind) {
    case KIND_FILE: out << "file"; break;
    case KIND_DIRECTORY: out << "directory"; break;
    case KIND_UNKNOWN: out << "unknown"; break;
    default: out << "!! EntryKind=" << static_cast<int>(kind);
  }
  return out;
}

bool kinds_are_different(EntryKind a, EntryKind b)
{
  return a != KIND_UNKNOWN && b != KIND_UNKNOWN && a != b;
}

FileSystemPayload::FileSystemPayload(ChannelID channel_id,
  FileSystemAction action,
  EntryKind entry_kind,
  string &&old_path,
  string &&path) :
  channel_id{channel_id},
  action{action},
  entry_kind{entry_kind},
  old_path{move(old_path)},
  path{move(path)}
{
  //
}

FileSystemPayload::FileSystemPayload(FileSystemPayload &&original) noexcept :
  channel_id{original.channel_id},
  action{original.action},
  entry_kind{original.entry_kind},
  old_path{move(original.old_path)},
  path{move(original.path)}
{
  //
}

string FileSystemPayload::describe() const
{
  ostringstream builder;
  builder << "[FileSystemPayload channel " << channel_id << " " << entry_kind;
  builder << " " << action;
  if (!old_path.empty()) {
    builder << " {" << old_path << " => " << path << "}";
  } else {
    builder << " " << path;
  }
  builder << "]";
  return builder.str();
}

CommandPayload::CommandPayload(CommandAction action,
  CommandID id,
  std::string &&root,
  uint_fast32_t arg) :
  id{id},
  action{action},
  root{move(root)},
  arg{arg}
{
  //
}

CommandPayload::CommandPayload(CommandPayload &&original) noexcept :
  id{original.id},
  action{original.action},
  root{move(original.root)},
  arg{original.arg}
{
  //
}

string CommandPayload::describe() const
{
  ostringstream builder;
  builder << "[CommandPayload id " << id << " ";

  switch (action) {
    case COMMAND_ADD: builder << "add " << root << " at channel " << arg; break;
    case COMMAND_REMOVE: builder << "remove channel " << arg; break;
    case COMMAND_LOG_FILE: builder << "log to file " << root; break;
    case COMMAND_LOG_DISABLE: builder << "disable logging"; break;
    case COMMAND_POLLING_INTERVAL: builder << "polling interval " << arg; break;
    case COMMAND_POLLING_THROTTLE: builder << "polling throttle " << arg; break;
    case COMMAND_DRAIN: builder << "drain"; break;
    default: builder << "!!action=" << action; break;
  }

  builder << "]";
  return builder.str();
}

AckPayload::AckPayload(CommandID key, ChannelID channel_id, bool success, string &&message) :
  key{key},
  channel_id{channel_id},
  success{success},
  message{move(message)}
{
  //
}

string AckPayload::describe() const
{
  ostringstream builder;
  builder << "[AckPayload ack " << key << "]";
  return builder.str();
}

const FileSystemPayload *Message::as_filesystem() const
{
  return kind == KIND_FILESYSTEM ? &filesystem_payload : nullptr;
}

const CommandPayload *Message::as_command() const
{
  return kind == KIND_COMMAND ? &command_payload : nullptr;
}

const AckPayload *Message::as_ack() const
{
  return kind == KIND_ACK ? &ack_payload : nullptr;
}

Message Message::ack(const Message &original, bool success, string &&message)
{
  const CommandPayload *payload = original.as_command();
  assert(payload != nullptr);

  return Message(AckPayload(payload->get_id(), payload->get_channel_id(), success, move(message)));
}

Message Message::ack(const Message &original, const Result<> &result)
{
  if (result.is_ok()) {
    return ack(original, true, "");
  }

  return ack(original, false, string(result.get_error()));
}

Message::Message(FileSystemPayload &&payload) : kind{KIND_FILESYSTEM}, filesystem_payload{move(payload)}
{
  //
}

Message::Message(CommandPayload &&payload) : kind{KIND_COMMAND}, command_payload{move(payload)}
{
  //
}

Message::Message(AckPayload &&payload) : kind{KIND_ACK}, ack_payload{move(payload)}
{
  //
}

Message::Message(Message &&original) noexcept : kind{original.kind}, pending{true}
{
  switch (kind) {
    case KIND_FILESYSTEM: new (&filesystem_payload) FileSystemPayload(move(original.filesystem_payload)); break;
    case KIND_COMMAND: new (&command_payload) CommandPayload(move(original.command_payload)); break;
    case KIND_ACK: new (&ack_payload) AckPayload(move(original.ack_payload)); break;
  };
}

Message::~Message()
{
  switch (kind) {
    case KIND_FILESYSTEM: filesystem_payload.~FileSystemPayload(); break;
    case KIND_COMMAND: command_payload.~CommandPayload(); break;
    case KIND_ACK: ack_payload.~AckPayload(); break;
  };
}

string Message::describe() const
{
  ostringstream builder;
  builder << "[Message ";

  switch (kind) {
    case KIND_FILESYSTEM: builder << filesystem_payload; break;
    case KIND_COMMAND: builder << command_payload; break;
    case KIND_ACK: builder << ack_payload; break;
    default: builder << "!!kind=" << kind; break;
  };

  builder << "]";
  return builder.str();
}

std::ostream &operator<<(std::ostream &stream, const FileSystemPayload &e)
{
  stream << e.describe();
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const CommandPayload &e)
{
  stream << e.describe();
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const AckPayload &e)
{
  stream << e.describe();
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const Message &e)
{
  stream << e.describe();
  return stream;
}
