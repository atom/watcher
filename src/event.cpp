#include <string>
#include <utility>

#include "event.h"

using std::move;

FileSystemEvent::FileSystemEvent(
  const FileSystemAction action,
  const EntryKind entryKind,
  const std::string &&dirname,
  const std::string &&oldBaseName,
  const std::string &&newBaseName
) :
  action{action},
  entryKind{entryKind},
  dirname{move(dirname)},
  oldBaseName{move(oldBaseName)},
  newBaseName{move(newBaseName)}
{
  //
}

CommandEvent::CommandEvent(const CommandAction action, const std::string &&root) :
  action{action},
  root{move(root)}
{
  //
}

AckEvent::AckEvent(const void* event) :
  event{event}
{
  //
}
