#include <string>
#include <sstream>
#include <iomanip>
#include <utility>

#include "event.h"

using std::move;
using std::string;
using std::ostringstream;
using std::hex;
using std::dec;

FileSystemEvent::FileSystemEvent(
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

FileSystemEvent(FileSystemEvent&& original) :
  action{original.action},
  entryKind{original.entryKind},
  dirname{move(original.dirname)},
  oldBaseName{move(original.oldBaseName)},
  newBaseName{move(original.newBaseName)}
{
  //
}

string FileSystemEvent::describe()
{
  ostringstream builder;
  builder << "[FileSystemEvent ";

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

CommandEvent::CommandEvent(const CommandAction action, const std::string &&root) :
  action{action},
  root{move(root)}
{
  //
}

CommandEvent(CommandEvent&& original) :
  action{original.action},
  root{move(original.root)}
{
  //
}

const CommandAction& CommandEvent::get_action()
{
  return action;
}

const string& CommandEvent::get_root()
{
  return root;
}

string CommandEvent::describe()
{
  ostringstream builder;
  builder << "[CommandEvent ";

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

  builder << " ]"
  return builder.str();
}

AckEvent::AckEvent(const void* event) :
  event{event}
{
  //
}

string AckEvent::describe()
{
  ostringstream builder;
  builder << "[AckEvent ack " << hex << event << " ]";
  return builder.str();
}

FileSystemEvent* Event::as_filesystem()
{
  return kind == KIND_FILESYSTEM ? &fsEvent : nullptr;
}

CommandEvent* Event::as_command()
{
  return kind == KIND_COMMAND ? &commandEvent : nullptr;
}

AckEvent* Event::as_ack()
{
  return kind == KIND_ACK ? &ackEvent : nullptr;
}

Event::Event(FileSystemEvent &&e) : kind{KIND_FILESYSTEM}, fsEvent{move(e)}
{
  //
}

Event::Event(CommandEvent &&e) : kind{KIND_COMMAND}, commandEvent{move(e)}
{
  //
}

Event::Event(AckEvent &&e) : kind{KIND_ACK}, ackEvent{move(e)}
{
  //
}

Event(Event&& original) : kind{original.kind}
{
  switch (kind) {
    case KIND_FILESYSTEM:
      fsEvent = move(original.fsEvent);
      break;
    case KIND_COMMAND:
      commandEvent = move(original.commandEvent);
      break;
    case KIND_ACK:
      ackEvent = move(original.ackEvent);
      break;
  };
}

string Event::describe()
{
  ostringstream builder;
  builder << "[Event id " << hex << this << dec << " ";

  switch (kind) {
    case KIND_FILESYSTEM:
      builder << fsEvent;
      break;
    case KIND_COMMAND:
      builder << commandEvent;
      break;
    case KIND_ACK:
      builder << ackEvent;
      break;
    default:
      builder << "!!kind=" << kind;
      break;
  };

  builder << "]";
  return builder.str();
}

std::ostream& operator<<(std::ostream& stream, const FileSystemEvent& e)
{
  stream << e.describe();
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const CommandEvent& e)
{
  stream << e.describe();
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const AckEvent& e)
{
  stream << e.describe();
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Event& e)
{
  stream << e.describe();
  return stream;
}
