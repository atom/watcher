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
  const EntryKind entryKind,
  const string &&dirname,
  const string &&oldBaseName,
  const string &&newBaseName
) :
  action{action},
  entryKind{entryKind},
  dirname{move(dirname)},
  oldBaseName{move(oldBaseName)},
  newBaseName{move(newBaseName)}
{
  //
}

FileSystemPayload::FileSystemPayload(FileSystemPayload&& original) :
  action{original.action},
  entryKind{original.entryKind},
  dirname{move(original.dirname)},
  oldBaseName{move(original.oldBaseName)},
  newBaseName{move(original.newBaseName)}
{
  //
}

string FileSystemPayload::describe() const
{
  ostringstream builder;
  builder << "[FileSystemPayload ";

  switch (entryKind) {
    case KIND_FILE:
      builder << "file ";
      break;
    case KIND_DIRECTORY:
      builder << "dir ";
      break;
    default:
      builder << "!!entryKind=" << entryKind << " ";
      break;
  }

  switch (action) {
    case ACTION_CREATED:
      builder << "created " << dirname << " " << oldBaseName;
      break;
    case ACTION_DELETED:
      builder << "deleted " << dirname << " " << oldBaseName;
      break;
    case ACTION_MODIFIED:
      builder << "modified " << dirname << " " << oldBaseName;
      break;
    case ACTION_RENAMED:
      builder << "renamed " << dirname << " {" << oldBaseName << " => " << newBaseName << "}";
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
  return kind == KIND_FILESYSTEM ? &fileSystemPayload : nullptr;
}

const CommandPayload* Message::as_command() const
{
  return kind == KIND_COMMAND ? &commandPayload : nullptr;
}

const AckPayload* Message::as_ack() const
{
  return kind == KIND_ACK ? &ackPayload : nullptr;
}

Message::Message(FileSystemPayload &&p) : kind{KIND_FILESYSTEM}, fileSystemPayload{move(p)}
{
  //
}

Message::Message(CommandPayload &&p) : kind{KIND_COMMAND}, commandPayload{move(p)}
{
  //
}

Message::Message(AckPayload &&p) : kind{KIND_ACK}, ackPayload{move(p)}
{
  //
}

Message::Message(Message&& original) : kind{original.kind}, pending{true}
{
  switch (kind) {
    case KIND_FILESYSTEM:
      new (&fileSystemPayload) FileSystemPayload(move(original.fileSystemPayload));
      break;
    case KIND_COMMAND:
      new (&commandPayload) CommandPayload(move(original.commandPayload));
      break;
    case KIND_ACK:
      new (&ackPayload) AckPayload(move(original.ackPayload));
      break;
  };
}

Message::~Message()
{
  switch (kind) {
    case KIND_FILESYSTEM:
      fileSystemPayload.~FileSystemPayload();
      break;
    case KIND_COMMAND:
      commandPayload.~CommandPayload();
      break;
    case KIND_ACK:
      ackPayload.~AckPayload();
      break;
  };
}

string Message::describe() const
{
  ostringstream builder;
  builder << "[Message id " << hex << this << dec << " ";

  switch (kind) {
    case KIND_FILESYSTEM:
      builder << fileSystemPayload;
      break;
    case KIND_COMMAND:
      builder << commandPayload;
      break;
    case KIND_ACK:
      builder << ackPayload;
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
