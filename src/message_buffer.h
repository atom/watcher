#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <string>
#include <utility>
#include <vector>

#include "message.h"

class MessageBuffer
{
public:
  MessageBuffer() = default;

  ~MessageBuffer() = default;

  using iter = std::vector<Message>::iterator;

  void created(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void modified(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void deleted(ChannelID channel_id, std::string &&path, const EntryKind &kind);

  void renamed(ChannelID channel_id, std::string &&old_path, std::string &&path, const EntryKind &kind);

  void ack(CommandID command_id, ChannelID channel_id, bool success, std::string &&msg);

  void error(ChannelID channel_id, std::string &&message, bool fatal);

  void reserve(size_t capacity) { messages.reserve(capacity); }

  void add(Message &&message) { messages.emplace_back(std::move(message)); }

  MessageBuffer::iter begin() { return messages.begin(); }

  MessageBuffer::iter end() { return messages.end(); }

  size_t size() { return messages.size(); }

  bool empty() { return messages.empty(); }

  MessageBuffer(const MessageBuffer &) = delete;
  MessageBuffer(MessageBuffer &&) = delete;
  MessageBuffer &operator=(const MessageBuffer &) = delete;
  MessageBuffer &operator=(MessageBuffer &&) = delete;

private:
  std::vector<Message> messages;
};

class ChannelMessageBuffer
{
public:
  ChannelMessageBuffer(MessageBuffer &buffer, ChannelID channel_id);
  ChannelMessageBuffer(const ChannelMessageBuffer &) = delete;
  ChannelMessageBuffer(ChannelMessageBuffer &&) = delete;
  ~ChannelMessageBuffer() = default;

  ChannelMessageBuffer &operator=(const ChannelMessageBuffer &) = delete;
  ChannelMessageBuffer &operator=(ChannelMessageBuffer &&) = delete;

  void created(std::string &&path, const EntryKind &kind) { buffer.created(channel_id, std::move(path), kind); }

  void modified(std::string &&path, const EntryKind &kind) { buffer.modified(channel_id, std::move(path), kind); }

  void deleted(std::string &&path, const EntryKind &kind) { buffer.deleted(channel_id, std::move(path), kind); }

  void renamed(std::string &&old_path, std::string &&path, const EntryKind &kind)
  {
    buffer.renamed(channel_id, std::move(old_path), std::move(path), kind);
  }

  void ack(CommandID command_id, bool success, std::string &&msg)
  {
    buffer.ack(command_id, channel_id, success, std::move(msg));
  }

  void error(std::string &&message, bool fatal) { buffer.error(channel_id, std::move(message), fatal); }

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
