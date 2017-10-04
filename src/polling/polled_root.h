#ifndef POLLED_ROOT_H
#define POLLED_ROOT_H

#include <string>
#include <memory>
#include <iostream>

#include "directory_record.h"
#include "polling_iterator.h"
#include "../message.h"

// Single root directory monitored by the `PollingThread`.
class PolledRoot {
public:
  // Begin watching a new root directory. Events produced by changes observed within this subtree should be
  // sent to `channel_id`.
  //
  // The newly constructed root does *not* contain any initial scan information, to avoid CPU usage spikes when
  // watching large directory trees. The subtree's records will be populated on the first scan.
  PolledRoot(std::string &&root_path, ChannelID channel_id);

  PolledRoot(const PolledRoot &) = delete;
  PolledRoot(PolledRoot &&) = delete;
  ~PolledRoot() = default;

  PolledRoot &operator=(const PolledRoot &) = delete;
  PolledRoot &operator=(PolledRoot &&) = delete;

  // Perform at most `throttle_allocation` operations, accumulating any changes into a provided `buffer` for batch
  // delivery. Return the number of operations actually performed.
  //
  // Iteration state is persisted within a `PollingIterator`, so subsequent calls to `PolledRoot::advance()` will pick
  // up where this call left off. When a complete scan is performed, the iteration will stop and the iterator will be
  // left ready to begin again at the root directory next time.
  size_t advance(MessageBuffer &buffer, size_t throttle_allocation);
private:
  // Recursive data structure used to remember the last stat results from the entire filesystem subhierarchy.
  std::shared_ptr<DirectoryRecord> root;

  // Events produced by changes within this root should by targetted for this channel.
  ChannelID channel_id;

  // Persistent iteration state.
  PollingIterator iterator;

  // Diagnostics and logging are your friend.
  friend std::ostream &operator<<(std::ostream &out, const PolledRoot &root)
  {
    return out << "PolledRoot{root=" << root.root->path() << " channel=" << root.channel_id << "}";
  }
};

#endif
