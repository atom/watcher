#ifndef COOKIE_JAR
#define COOKIE_JAR

#include <string>
#include <map>
#include <deque>
#include <utility>
#include <memory>
#include <sys/types.h>

#include "../../message.h"
#include "../../message_buffer.h"

class Cookie {
public:
  Cookie(ChannelID channel_id, std::string &&from_path, EntryKind kind);
  Cookie(Cookie &&other);
  Cookie(const Cookie &other) = delete;
  Cookie &operator=(Cookie &&cookie) = delete;
  Cookie &operator=(const Cookie &other) = delete;

  const ChannelID &get_channel_id() const { return channel_id; }

  std::string get_from_path() { return std::string(std::move(from_path)); }

  const EntryKind &get_kind() { return kind; }

  bool is_null() const { return channel_id == NULL_CHANNEL_ID; }

private:
  const ChannelID channel_id;
  std::string from_path;
  const EntryKind kind;
};

class CookieBatch {
public:
  void moved_from(
    MessageBuffer &messages,
    ChannelID channel_id,
    uint32_t cookie,
    std::string &&old_path,
    EntryKind kind
  );

  std::unique_ptr<Cookie> yoink(uint32_t cookie);

  void flush(MessageBuffer &messages);

  bool empty() const { return from_paths.empty(); }

private:
  std::map<uint32_t, Cookie> from_paths;
};

class CookieJar {
public:
  CookieJar(unsigned int max_batches = 2);
  CookieJar(const CookieJar &other) = delete;
  CookieJar(CookieJar &&other) = delete;
  CookieJar &operator=(const CookieJar &other) = delete;
  CookieJar &operator=(CookieJar &&other) = delete;

  void moved_from(
    MessageBuffer &messages,
    ChannelID channel_id,
    uint32_t cookie,
    std::string &&old_path,
    EntryKind kind
  );

  void moved_to(
    MessageBuffer &messages,
    ChannelID channel_id,
    uint32_t cookie,
    std::string &&new_path,
    EntryKind kind
  );

  void flush_oldest_batch(MessageBuffer &messages);

private:
  std::deque<CookieBatch> batches;
};

#endif
