#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <string>
#include <vector>
#include <utility>

#include "message.h"

class MessageBuffer {
public:
  MessageBuffer() = default;
  MessageBuffer(const MessageBuffer &) = delete;
  MessageBuffer(MessageBuffer &&) = delete;

  MessageBuffer &operator=(const MessageBuffer &) = delete;
  MessageBuffer &operator=(MessageBuffer &&) = delete;

  typedef std::vector<Message>::iterator iter;

  void created(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void modified(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void deleted(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void renamed(ChannelID channel_id, std::string &&old_path, std::string &&path, const EntryKind &kind);

  void ack(CommandID command_id, ChannelID channel_id, bool success, const std::string &&message);

  void reserve(size_t capacity) { messages.reserve(capacity); }

  MessageBuffer::iter begin() { return messages.begin(); }

  MessageBuffer::iter end() { return messages.end(); }

  size_t size() { return messages.size(); }

  bool empty() { return messages.empty(); }

private:
  std::vector<Message> messages;
};

class ChannelMessageBuffer {
public:
  ChannelMessageBuffer(MessageBuffer &buffer, ChannelID channel_id);
  ChannelMessageBuffer(const ChannelMessageBuffer &) = delete;
  ChannelMessageBuffer(ChannelMessageBuffer &&) = delete;

  ChannelMessageBuffer &operator=(const ChannelMessageBuffer &) = delete;
  ChannelMessageBuffer &operator=(ChannelMessageBuffer &&) = delete;

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

  void ack(CommandID command_id, bool success, const std::string &&message)
  {
    buffer.ack(command_id, channel_id, success, std::move(message));
  }

  void reserve(size_t capacity) { buffer.reserve(capacity); }

  MessageBuffer::iter begin() { return buffer.begin(); }

  MessageBuffer::iter end() { return buffer.end(); }

  size_t size() { return buffer.size(); }

  bool empty() { return buffer.empty(); }

  ChannelID get_channel_id() { return channel_id; }

private:
  ChannelID channel_id;
  MessageBuffer &buffer;
};

#endif
