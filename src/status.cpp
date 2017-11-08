#include <iomanip>
#include <iostream>

#include "log.h"
#include "status.h"

using std::endl;
using std::ostream;

void Status::assimilate_worker_status(const Status &other)
{
  worker_thread_state = other.worker_thread_state;
  worker_thread_ok = other.worker_thread_ok;
  worker_in_size = other.worker_in_size;
  worker_in_ok = other.worker_in_ok;
  worker_out_size = other.worker_out_size;
  worker_out_ok = other.worker_out_ok;

  worker_subscription_count = other.worker_subscription_count;
#ifdef PLATFORM_MACOS
  worker_rename_buffer_size = other.worker_rename_buffer_size;
  worker_recent_file_cache_size = other.worker_recent_file_cache_size;
#endif
#ifdef PLATFORM_LINUX
  worker_watch_descriptor_count = other.worker_watch_descriptor_count;
  worker_channel_count = other.worker_channel_count;
  worker_cookie_jar_size = other.worker_cookie_jar_size;
#endif

  worker_received = true;
}

void Status::assimilate_polling_status(const Status &other)
{
  polling_thread_state = other.polling_thread_state;
  polling_thread_ok = other.polling_thread_ok;
  polling_in_size = other.polling_in_size;
  polling_in_ok = other.polling_in_ok;
  polling_out_size = other.polling_out_size;
  polling_out_ok = other.polling_out_ok;

  polling_root_count = other.polling_root_count;
  polling_entry_count = other.polling_entry_count;

  polling_received = true;
}

ostream &operator<<(ostream &out, const Status &status)
{
  out << "WATCHER STATUS SUMMARY\n"
      << "* main thread:\n"
      << "  - " << plural(status.pending_callback_count, "pending callback") << "\n"
      << "  - " << plural(status.channel_callback_count, "channel callback") << "\n"
      << "* worker thread:\n"
      << "  - state: " << status.worker_thread_state << "\n"
      << "  - health: " << status.worker_thread_ok << "\n"
      << "  - in queue health: " << status.worker_in_ok << "\n"
      << "  - " << plural(status.worker_in_size, "in queue message") << "\n"
      << "  - out queue health: " << status.worker_out_ok << "\n"
      << "  - " << plural(status.worker_out_size, "out queue message") << "* polling thread\n"
      << "  - " << plural(status.worker_subscription_count, "subscription") << endl;
#ifdef PLATFORM_MACOS
  out << "  - " << plural(status.worker_rename_buffer_size, "rename buffer entry", "rename buffer entries") << "\n"
      << "  - " << plural(status.worker_recent_file_cache_size, "recent cache entry", "recent cache entries") << "\n";
#endif
#ifdef PLATFORM_LINUX
  out << "  - " << plural(status.worker_watch_descriptor_count, "active watch descriptor") << "\n"
      << "  - " << plural(status.worker_channel_count, "channel") << "\n"
      << "  - " << plural(status.worker_cookie_jar_size, "cookies") << "\n";
#endif
  out << "* polling thread\n"
      << "  - state: " << status.polling_thread_state << "\n"
      << "  - health: " << status.polling_thread_ok << "\n"
      << "  - in queue health: " << status.worker_in_ok << "\n"
      << "  - " << plural(status.polling_in_size, "in queue message") << "\n"
      << "  - out queue health: " << status.worker_out_ok << "\n"
      << "  - " << plural(status.polling_out_size, "out queue message") << "\n"
      << "  - " << plural(status.polling_root_count, "polled root") << "\n"
      << "  - " << plural(status.polling_entry_count, "polled entry", "polled entries") << "\n"
      << endl;
  return out;
}
