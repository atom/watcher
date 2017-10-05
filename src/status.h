#ifndef STATUS_H
#define STATUS_H

#include <string>
#include <iostream>

// Summarize the module's health. This includes information like the health of all Errable and SyncErrable
// resources and the sizes of internal queues and buffers.
class Status {
public:
  // Main thread
  size_t pending_callback_count;
  size_t channel_callback_count;

  // Worker thread
  std::string worker_thread_state;
  std::string worker_thread_ok;
  size_t worker_in_size;
  std::string worker_in_ok;
  size_t worker_out_size;
  std::string worker_out_ok;

  // Polling thread
  std::string polling_thread_state;
  std::string polling_thread_ok;
  size_t polling_in_size;
  std::string polling_in_ok;
  size_t polling_out_size;
  std::string polling_out_ok;
};

std::ostream &operator<<(std::ostream &out, const Status &status);

#endif
