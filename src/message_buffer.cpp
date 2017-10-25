#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "log.h"
#include "message.h"
#include "message_buffer.h"

using std::endl;
using std::move;
using std::string;

void MessageBuffer::created(ChannelID channel_id, std::string &&path, const EntryKind &kind)
{
  Message message(FileSystemPayload::created(channel_id, move(path), kind));
  LOGGER << "Emitting filesystem message " << message << endl;
  messages.push_back(move(message));
}

void MessageBuffer::modified(ChannelID channel_id, std::string &&path, const EntryKind &kind)
{
  Message message(FileSystemPayload::modified(channel_id, move(path), kind));
  LOGGER << "Emitting filesystem message " << message << endl;
  messages.push_back(move(message));
}

void MessageBuffer::deleted(ChannelID channel_id, std::string &&path, const EntryKind &kind)
{
  Message message(FileSystemPayload::deleted(channel_id, move(path), kind));
  LOGGER << "Emitting filesystem message " << message << endl;
  messages.push_back(move(message));
}

void MessageBuffer::renamed(ChannelID channel_id, std::string &&old_path, std::string &&path, const EntryKind &kind)
{
  Message message(FileSystemPayload::renamed(channel_id, move(old_path), move(path), kind));
  LOGGER << "Emitting filesystem message " << message << endl;
  messages.push_back(move(message));
}

void MessageBuffer::ack(CommandID command_id, ChannelID channel_id, bool success, string &&msg)
{
  Message message(AckPayload(command_id, channel_id, success, move(msg)));
  LOGGER << "Emitting ack message " << message << endl;
  messages.push_back(move(message));
}

void MessageBuffer::error(ChannelID channel_id, string &&message, bool fatal)
{
  Message m(ErrorPayload(channel_id, move(message), fatal));
  LOGGER << "Emitting error message " << m << endl;
  messages.push_back(move(m));
}

ChannelMessageBuffer::ChannelMessageBuffer(MessageBuffer &buffer, ChannelID channel_id) :
  channel_id{channel_id},
  buffer{buffer} {
    //
  };
