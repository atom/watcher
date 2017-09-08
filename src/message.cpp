#include <string>
#include <sstream>
#include <iomanip>
#include <utility>

#include "message.h"

using std::move;
using std::string;
using std::ostringstream;
using std::ostream;
using std::hex;
using std::dec;

ostream &operator<<(ostream &out, FileSystemAction action)
{
  switch (action) {
    case ACTION_CREATED:
      out << "created";
      break;
    case ACTION_DELETED:
      out << "deleted";
      break;
    case ACTION_MODIFIED:
      out << "modified";
      break;
    case ACTION_RENAMED:
      out << "renamed";
      break;
    default:
      out << "!! FileSystemAction=" << static_cast<int>(action);
  }
  return out;
}

ostream &operator<<(ostream &out, EntryKind kind)
{
  switch (kind) {
    case KIND_FILE:
      out << "file";
      break;
    case KIND_DIRECTORY:
      out << "directory";
      break;
    case KIND_UNKNOWN:
      out << "unknown";
      break;
    default:
      out << "!! EntryKind=" << static_cast<int>(kind);
  }
  return out;
}

bool kinds_are_different(EntryKind a, EntryKind b)
{
  return a != KIND_UNKNOWN && b != KIND_UNKNOWN && a != b;
}

FileSystemPayload::FileSystemPayload(
  const ChannelID channel_id,
  const FileSystemAction action,
  const EntryKind entry_kind,
  const string &&old_path,
  const string &&path
) :
  channel_id{channel_id},
  action{action},
  entry_kind{entry_kind},
  old_path{move(old_path)},
  path{move(path)}
{
  //
}

FileSystemPayload::FileSystemPayload(FileSystemPayload &&original) :
  channel_id{original.channel_id},
  action{original.action},
  entry_kind{original.entry_kind},
  old_path{move(original.old_path)},
  new_path{move(original.new_path)}
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

CommandPayload::CommandPayload(
  const CommandID id,
  const CommandAction action,
  const std::string &&root,
  const ChannelID channel_id
) :
  id{id},
  action{action},
  root{move(root)},
  channel_id{channel_id}
{
  //
}

CommandPayload::CommandPayload(CommandPayload &&original) :
  id{original.id},
  action{original.action},
  root{move(original.root)},
  channel_id{original.channel_id}
{
  //
}

string CommandPayload::describe() const
{
  ostringstream builder;
  builder << "[CommandPayload id " << id << " ";

  switch (action) {
    case COMMAND_ADD:
      builder << "add " << root << " at channel " << channel_id;
      break;
    case COMMAND_REMOVE:
      builder << "remove channel " << channel_id;
      break;
    case COMMAND_LOG_FILE:
      builder << "log to file " << root;
      break;
    case COMMAND_LOG_DISABLE:
      builder << "disable logging";
      break;
    default:
      builder << "!!action=" << action;
      break;
  }

  builder << "]";
  return builder.str();
}

AckPayload::AckPayload(const CommandID key, const ChannelID channel_id, bool success, const string &&message) :
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

const FileSystemPayload* Message::as_filesystem() const
{
  return kind == KIND_FILESYSTEM ? &filesystem_payload : nullptr;
}

const CommandPayload* Message::as_command() const
{
  return kind == KIND_COMMAND ? &command_payload : nullptr;
}

const AckPayload* Message::as_ack() const
{
  return kind == KIND_ACK ? &ack_payload : nullptr;
}

Message::Message(FileSystemPayload &&p) : kind{KIND_FILESYSTEM}, filesystem_payload{move(p)}
{
  //
}

Message::Message(CommandPayload &&p) : kind{KIND_COMMAND}, command_payload{move(p)}
{
  //
}

Message::Message(AckPayload &&p) : kind{KIND_ACK}, ack_payload{move(p)}
{
  //
}

Message::Message(Message&& original) : kind{original.kind}, pending{true}
{
  switch (kind) {
    case KIND_FILESYSTEM:
      new (&filesystem_payload) FileSystemPayload(move(original.filesystem_payload));
      break;
    case KIND_COMMAND:
      new (&command_payload) CommandPayload(move(original.command_payload));
      break;
    case KIND_ACK:
      new (&ack_payload) AckPayload(move(original.ack_payload));
      break;
  };
}

Message::~Message()
{
  switch (kind) {
    case KIND_FILESYSTEM:
      filesystem_payload.~FileSystemPayload();
      break;
    case KIND_COMMAND:
      command_payload.~CommandPayload();
      break;
    case KIND_ACK:
      ack_payload.~AckPayload();
      break;
  };
}

string Message::describe() const
{
  ostringstream builder;
  builder << "[Message ";

  switch (kind) {
    case KIND_FILESYSTEM:
      builder << filesystem_payload;
      break;
    case KIND_COMMAND:
      builder << command_payload;
      break;
    case KIND_ACK:
      builder << ack_payload;
      break;
    default:
      builder << "!!kind=" << kind;
      break;
  };

  builder << "]";
  return builder.str();
}

std::ostream& operator<<(std::ostream& stream, const FileSystemPayload& e)
{
  stream << e.describe();
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const CommandPayload& e)
{
  stream << e.describe();
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const AckPayload& e)
{
  stream << e.describe();
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Message& e)
{
  stream << e.describe();
  return stream;
}
