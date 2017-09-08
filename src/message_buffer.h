#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <string>
#include <vector>
#include <utility>

#include "message.h"

class MessageBuffer {
public:
  typedef std::vector<Message>::iterator iter;

  void created(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void modified(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void deleted(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void renamed(ChannelID channel_id, std::string &&old_path, std::string &&path, const EntryKind &kind);

  MessageBuffer::iter begin() { return messages.begin(); }

  MessageBuffer::iter end() { return messages.end(); }

  bool empty() { return messages.empty(); }

private:
  std::vector<Message> messages;
};

class ChannelMessageBuffer {
public:
  ChannelMessageBuffer(ChannelID channel_id) : channel_id{channel_id} {};

  void created(std::string &&path, const EntryKind &kind)
  {
    buffer.created(channel_id, std::move(path), kind);
  }

  void modified(std::string &&path, const EntryKind &kind)
  {
    buffer.modified(channel_id, std::move(path), kind);
  }

  void deleted(std::string &&path, const EntryKind &kind)
  {
    buffer.deleted(channel_id, std::move(path), kind);
  }

  void renamed(std::string &&old_path, std::string &&path, const EntryKind &kind)
  {
    buffer.renamed(channel_id, std::move(old_path), std::move(path), kind);
  }

  MessageBuffer::iter begin() { return buffer.begin(); }

  MessageBuffer::iter end() { return buffer.end(); }

  bool empty() { return buffer.empty(); }

private:
  ChannelID channel_id;
  MessageBuffer buffer;
};

#endif
