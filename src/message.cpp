#include <string>
#include <sstream>
#include <iomanip>
#include <utility>

#include "message.h"

using std::move;
using std::string;
using std::ostringstream;
using std::hex;
using std::dec;

FileSystemPayload::FileSystemPayload(
  const FileSystemAction action,
  const EntryKind entry_kind,
  const string &&dirname,
  const string &&old_base_name,
  const string &&new_base_name
) :
  action{action},
  entry_kind{entry_kind},
  dirname{move(dirname)},
  old_base_name{move(old_base_name)},
  new_base_name{move(new_base_name)}
{
  //
}

FileSystemPayload::FileSystemPayload(FileSystemPayload&& original) :
  action{original.action},
  entry_kind{original.entry_kind},
  dirname{move(original.dirname)},
  old_base_name{move(original.old_base_name)},
  new_base_name{move(original.new_base_name)}
{
  //
}

string FileSystemPayload::describe() const
{
  ostringstream builder;
  builder << "[FileSystemPayload ";

  switch (entry_kind) {
    case KIND_FILE:
      builder << "file ";
      break;
    case KIND_DIRECTORY:
      builder << "dir ";
      break;
    default:
      builder << "!!entry_kind=" << entry_kind << " ";
      break;
  }

  switch (action) {
    case ACTION_CREATED:
      builder << "created " << dirname << " " << old_base_name;
      break;
    case ACTION_DELETED:
      builder << "deleted " << dirname << " " << old_base_name;
      break;
    case ACTION_MODIFIED:
      builder << "modified " << dirname << " " << old_base_name;
      break;
    case ACTION_RENAMED:
      builder << "renamed " << dirname << " {" << old_base_name << " => " << new_base_name << "}";
      break;
    default:
      builder << "!!action=" << action << " ";
      break;
  }

  builder << " ]";
  return builder.str();
}

CommandPayload::CommandPayload(const CommandAction action, const std::string &&root) :
  action{action},
  root{move(root)}
{
  //
}

CommandPayload::CommandPayload(CommandPayload&& original) :
  action{original.action},
  root{move(original.root)}
{
  //
}

const CommandAction& CommandPayload::get_action() const
{
  return action;
}

const string& CommandPayload::get_root() const
{
  return root;
}

string CommandPayload::describe() const
{
  ostringstream builder;
  builder << "[CommandPayload ";

  switch (action) {
    case COMMAND_ADD:
      builder << "add " << root;
      break;
    case COMMAND_REMOVE:
      builder << "remove " << root;
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

  builder << " ]";
  return builder.str();
}

AckPayload::AckPayload(const void* event) :
  event{event}
{
  //
}

string AckPayload::describe() const
{
  ostringstream builder;
  builder << "[AckPayload ack " << hex << event << " ]";
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
  builder << "[Message id " << hex << this << dec << " ";

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
