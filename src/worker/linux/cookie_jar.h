#ifndef COOKIE_JAR
#define COOKIE_JAR

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <sys/types.h>
#include <utility>

#include "../../message.h"
#include "../../message_buffer.h"
#include "../recent_file_cache.h"

// Remember a path that was observed in an IN_MOVED_FROM inotify event until its corresponding IN_MOVED_TO event
// is observed, or until it times out.
class Cookie
{
public:
  Cookie(ChannelID channel_id, std::string &&from_path, EntryKind kind);
  Cookie(Cookie &&other) noexcept;
  ~Cookie() = default;

  const ChannelID &get_channel_id() const { return channel_id; }

  // Access the absolute path from this event.
  const std::string &get_from_path() { return from_path; }

  // Take possession of the absolute path from this event.
  std::string move_from_path() { return std::string(std::move(from_path)); }

  const EntryKind &get_kind() { return kind; }

  Cookie(const Cookie &other) = delete;
  Cookie &operator=(Cookie &&cookie) = delete;
  Cookie &operator=(const Cookie &other) = delete;

private:
  const ChannelID channel_id;
  std::string from_path;
  const EntryKind kind;
};

// Collection of Cookies observed from rename events within a single cycle of inotify events. Batches are used to
// age off rename events that haven't been matched after a fixed number of inotify deliveries.
class CookieBatch
{
public:
  CookieBatch() = default;
  ~CookieBatch() = default;

  // Insert a new Cookie to eventually match an IN_MOVED_FROM event. If an existing Cookie already exists for this
  // cookie value, immediately age the old Cookie off and buffer a deletion event.
  void moved_from(MessageBuffer &messages,
    ChannelID channel_id,
    uint32_t cookie,
    std::string &&old_path,
    EntryKind kind);

  // Remove a Cookie from this batch that has the specified cookie value. Return nullptr instead if no such cookie
  // exists.
  std::unique_ptr<Cookie> yoink(uint32_t cookie);

  // Age off all Cookies within this batch by buffering them as deletion events. Evict them from the cache.
  void flush(MessageBuffer &messages, RecentFileCache &cache);

  bool empty() const { return from_paths.empty(); }

  CookieBatch(const CookieBatch &) = delete;
  CookieBatch(CookieBatch &&) = delete;
  CookieBatch &operator=(const CookieBatch &) = delete;
  CookieBatch &operator=(CookieBatch &&) = delete;

private:
  std::map<uint32_t, Cookie> from_paths;
};

// Associate IN_MOVED_FROM and IN_MOVED_TO events from inotify received within a configurable number of consecutive
// notification cycles. The CookieJar contains a fixed number of CookieBatches that contain unmatched IN_MOVED_FROM
// events collected within a single notification cycle. As more notifications arrive or read() calls time out, events
// that remain unmatched are aged off and emitted as deletion events.
class CookieJar
{
public:
  // Construct a CookieJar capable of correlating rename events across `max_batches` consecutive inotify event cycles.
  // Specifying a higher number of batches improves the watcher's ability to match rename events that occur at
  // high rates, at the cost of increasing memory usage and the latency of delete events delivered when an entry
  // is renamed outside of a watched directory. If `max_batches` is 0, _no_ rename correlation will be done at
  // all; all renames will be emitted as create/delete pairs instead. If `max_batches` is 1, only rename events
  // that arrive within the same notification cycle will be caught, but deletion events will be delivered with
  // no additional latency. The default of 2 correlates rename events across consecutive notification cycles but
  // no further.
  explicit CookieJar(unsigned int max_batches = 2);
  ~CookieJar() = default;

  // Observe an IN_MOVED_FROM event by adding a Cookie to the freshest CookieBatch.
  void moved_from(MessageBuffer &messages,
    ChannelID channel_id,
    uint32_t cookie,
    std::string &&old_path,
    EntryKind kind);

  // Observe an IN_MOVED_TO event. Search the current CookieBatches for a recent IN_MOVED_FROM event with a matching
  // `cookie` value. If no match is found, emit a creation event for the entry. If a match is found but the channel
  // or entry kind don't match, emit a delete/create event pair for the old and new entries. Otherwise, emit the
  // successfully correlated rename event.
  void moved_to(MessageBuffer &messages, ChannelID channel_id, uint32_t cookie, std::string &&new_path, EntryKind kind);

  // Buffer deletion events for any Cookies that have not been matched within `max_batches` CookieBatches. Add a
  // fresh CookieBatch to capture the next cycle of rename events.
  void flush_oldest_batch(MessageBuffer &messages, RecentFileCache &cache);

  CookieJar(const CookieJar &other) = delete;
  CookieJar(CookieJar &&other) = delete;
  CookieJar &operator=(const CookieJar &other) = delete;
  CookieJar &operator=(CookieJar &&other) = delete;

private:
  std::deque<CookieBatch> batches;
};

#endif
