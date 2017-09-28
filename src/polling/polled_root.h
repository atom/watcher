#ifndef POLLED_ROOT_H
#define POLLED_ROOT_H

#include <string>
#include <memory>
#include <iostream>

#include "directory_record.h"
#include "polling_iterator.h"
#include "../message.h"

class PolledRoot {
public:
  PolledRoot(std::string &&root_path, ChannelID channel_id);
  PolledRoot(const PolledRoot &) = delete;
  PolledRoot(PolledRoot &&) = delete;
  ~PolledRoot();

  PolledRoot &operator=(const PolledRoot &) = delete;
  PolledRoot &operator=(PolledRoot &&) = delete;

  size_t advance(MessageBuffer &buffer, size_t throttle_allocation);
private:
  std::shared_ptr<DirectoryRecord> root;
  ChannelID channel_id;

  PollingIterator iterator;

  friend std::ostream &operator<<(std::ostream &out, const PolledRoot &root)
  {
    return out << "PolledRoot{root=" << root.root->path() << " channel=" << root.channel_id << "}";
  }
};

#endif
