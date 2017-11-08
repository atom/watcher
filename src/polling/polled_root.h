#ifndef POLLED_ROOT_H
#define POLLED_ROOT_H

#include <iostream>
#include <memory>
#include <string>

#include "../message.h"
#include "directory_record.h"
#include "polling_iterator.h"

// Single root directory monitored by the `PollingThread`.
class PolledRoot
{
public:
  // Begin watching a new root directory. Events produced by changes observed within this subtree should be
  // sent to `channel_id`.
  //
  // The newly constructed root does *not* contain any initial scan information, to avoid CPU usage spikes when
  // watching large directory trees. The subtree's records will be populated on the first scan.
  PolledRoot(std::string &&root_path, ChannelID channel_id, bool recursive);

  ~PolledRoot() = default;

  // Perform at most `throttle_allocation` operations, accumulating any changes into a provided `buffer` for batch
  // delivery. Return the number of operations actually performed.
  //
  // Iteration state is persisted within a `PollingIterator`, so subsequent calls to `PolledRoot::advance()` will pick
  // up where this call left off. When a complete scan is performed, the iteration will stop and the iterator will be
  // left ready to begin again at the root directory next time.
  size_t advance(MessageBuffer &buffer, size_t throttle_allocation);

  // Return `true` once the first complete scan has been completed by calls to `PolledRoot::advance()`.
  bool is_all_populated() { return all_populated; }

  // Count the number of filesystem entries that are covered by this polling thread.
  size_t count_entries() const;

  PolledRoot(const PolledRoot &) = delete;
  PolledRoot(PolledRoot &&) = delete;
  PolledRoot &operator=(const PolledRoot &) = delete;
  PolledRoot &operator=(PolledRoot &&) = delete;

private:
  // Recursive data structure used to remember the last stat results from the entire filesystem subhierarchy.
  std::shared_ptr<DirectoryRecord> root;

  // Events produced by changes within this root should by targetted for this channel.
  ChannelID channel_id;

  // Persistent iteration state.
  PollingIterator iterator;

  // Becomes `true` when the first full subtree scan has completed.
  bool all_populated;

  // Diagnostics and logging are your friend.
  friend std::ostream &operator<<(std::ostream &out, const PolledRoot &root)
  {
    return out << "PolledRoot{root=" << root.root->path() << " channel=" << root.channel_id << "}";
  }
};

#endif
