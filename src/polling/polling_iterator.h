#ifndef POLLING_ITERATOR
#define POLLING_ITERATOR

#include <memory>
#include <utility>
#include <string>
#include <stack>
#include <queue>
#include <iostream>
#include <uv.h>

#include "../message.h"
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

  std::string current_path;
  std::vector<Entry> entries;
  std::vector<Entry>::iterator current_entry;

  std::queue<std::shared_ptr<DirectoryRecord>> directories;

  enum {
    SCAN,
    ENTRIES,
    RESET
  } phase;

  friend class BoundPollingIterator;

  friend std::ostream &operator<<(std::ostream &out, const PollingIterator &iterator)
  {
    out << "PollingIterator{at ";
    out << iterator.current_path;
    out << " phase=";
    switch (iterator.phase) {
      case SCAN: out << "SCAN"; break;
      case ENTRIES: out << "ENTRIES"; break;
      case RESET: out << "RESET"; break;
      default: out << "!!phase=" << iterator.phase; break;
    }
    out << " entries=" << iterator.entries.size();
    out << " directories=" << iterator.directories.size();
    out << "}";
    return out;
  }
};

class BoundPollingIterator {
public:
  BoundPollingIterator(PollingIterator &iterator, ChannelMessageBuffer &buffer);
  BoundPollingIterator(const BoundPollingIterator &) = delete;
  BoundPollingIterator(BoundPollingIterator &&) = delete;
  ~BoundPollingIterator();

  BoundPollingIterator &operator=(const BoundPollingIterator &) = delete;
  BoundPollingIterator &operator=(BoundPollingIterator &&) = delete;

  void push_entry(const std::string &&entry, EntryKind kind) { iterator.entries.emplace_back(std::move(entry), kind); }
  void push_directory(std::shared_ptr<DirectoryRecord> subdirectory) { iterator.directories.push(subdirectory); }
  ChannelMessageBuffer &get_buffer() { return buffer; }

  size_t advance(size_t throttle_allocation);
private:
  void advance_scan();
  void advance_entry();

  ChannelMessageBuffer &buffer;
  PollingIterator &iterator;

  friend std::ostream &operator<<(std::ostream &out, const BoundPollingIterator &it)
  {
    return out << "Bound{channel=" << it.buffer.get_channel_id() << " " << it.iterator << "}";
  }
};

#endif