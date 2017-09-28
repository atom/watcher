#ifndef POLLING_ITERATOR
#define POLLING_ITERATOR

#include <memory>
#include <string>
#include <stack>
#include <queue>
#include <uv.h>

#include "../message_buffer.h"

class DirectoryRecord;

class PollingIterator {
public:
  explicit PollingIterator(std::shared_ptr<DirectoryRecord> root);
  PollingIterator(const PollingIterator &) = delete;
  PollingIterator(PollingIterator &&) = delete;
  ~PollingIterator();

  PollingIterator &operator=(const PollingIterator &) = delete;
  PollingIterator &operator=(PollingIterator &&) = delete;
private:
  std::shared_ptr<DirectoryRecord> root;
  std::shared_ptr<DirectoryRecord> current;

  std::vector<std::string> entries;
  std::vector<std::string>::iterator current_entry;

  std::queue<std::shared_ptr<DirectoryRecord>> directories;

  enum {
    SCAN,
    ENTRIES,
    RESET
  } phase;

  friend class BoundPollingIterator;
};

class BoundPollingIterator {
public:
  BoundPollingIterator(PollingIterator &iterator, ChannelMessageBuffer &buffer);
  BoundPollingIterator(const BoundPollingIterator &) = delete;
  BoundPollingIterator(BoundPollingIterator &&) = delete;
  ~BoundPollingIterator();

  BoundPollingIterator &operator=(const BoundPollingIterator &) = delete;
  BoundPollingIterator &operator=(BoundPollingIterator &&) = delete;

  void push_entry(const std::string &entry) { iterator.entries.push_back(entry); }
  void push_directory(std::shared_ptr<DirectoryRecord> subdirectory) { iterator.directories.push(subdirectory); }
  ChannelMessageBuffer &get_buffer() { return buffer; }

  size_t advance(size_t throttle_allocation);
private:
  void advance_scan();
  void advance_entry();

  ChannelMessageBuffer &buffer;
  PollingIterator &iterator;
};

#endif
