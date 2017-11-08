#include <iomanip>
#include <iostream>

#include "log.h"
#include "status.h"

using std::endl;
using std::ostream;

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
