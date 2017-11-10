#ifndef STATUS_H
#define STATUS_H

#include <iostream>
#include <string>

// Summarize the module's health. This includes information like the health of all Errable resources and the sizes of
// internal queues and buffers.
class Status
{
public:
  // Main thread
  size_t pending_callback_count{0};
  size_t channel_callback_count{0};

  // Worker thread
  std::string worker_thread_state{};
  std::string worker_thread_ok{};
  size_t worker_in_size{0};
  std::string worker_in_ok{};
  size_t worker_out_size{0};
  std::string worker_out_ok{};

  size_t worker_subscription_count{0};
#ifdef PLATFORM_MACOS
  size_t worker_rename_buffer_size{0};
  size_t worker_recent_file_cache_size{0};
#endif
#ifdef PLATFORM_LINUX
  size_t worker_watch_descriptor_count{0};
  size_t worker_channel_count{0};
  size_t worker_cookie_jar_size{0};
#endif

  // Polling thread
  std::string polling_thread_state{};
  std::string polling_thread_ok{};
  size_t polling_in_size{0};
  std::string polling_in_ok{};
  size_t polling_out_size{0};
  std::string polling_out_ok{};

  size_t polling_root_count{0};
  size_t polling_entry_count{0};

  bool worker_received{false};
  bool polling_received{false};

  void assimilate_worker_status(const Status &other);

  void assimilate_polling_status(const Status &other);

  bool complete() { return worker_received && polling_received; }
};

std::ostream &operator<<(std::ostream &out, const Status &status);

#endif
