#include <string>
#include <vector>
#include <utility>
#include <iostream>

#include "log.h"
#include "message.h"
#include "message_buffer.h"

using std::string;
using std::move;
using std::endl;

void MessageBuffer::created(ChannelID channel_id, std::string &&path, const EntryKind &kind)
{
  FileSystemPayload payload(channel_id, ACTION_CREATED, kind, "", move(path));
  Message message(move(payload));

  LOGGER << "Emitting filesystem message " << message << endl;

  messages.push_back(move(message));
}

void MessageBuffer::modified(ChannelID channel_id, std::string &&path, const EntryKind &kind)
{
  FileSystemPayload payload(channel_id, ACTION_MODIFIED, kind, "", move(path));
  Message message(move(payload));

  LOGGER << "Emitting filesystem message " << message << endl;

  messages.push_back(move(message));
}

void MessageBuffer::deleted(ChannelID channel_id, std::string &&path, const EntryKind &kind)
{
  FileSystemPayload payload(channel_id, ACTION_DELETED, kind, "", move(path));
  Message message(move(payload));

  LOGGER << "Emitting filesystem message " << message << endl;

  messages.push_back(move(message));
}

void MessageBuffer::renamed(ChannelID channel_id, std::string &&old_path, std::string &&path, const EntryKind &kind)
{
  FileSystemPayload payload(channel_id, ACTION_RENAMED, kind, move(old_path), move(path));
  Message message(move(payload));

  LOGGER << "Emitting filesystem message " << message << endl;

  messages.push_back(move(message));
}
