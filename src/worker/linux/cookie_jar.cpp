#include <deque>
#include <map>
#include <memory>
#include <string>
#include <sys/types.h>
#include <utility>

#include "../../message.h"
#include "../../message_buffer.h"
#include "../recent_file_cache.h"
#include "cookie_jar.h"

using std::move;
using std::string;
using std::unique_ptr;

Cookie::Cookie(ChannelID channel_id, std::string &&from_path, EntryKind kind) noexcept :
  channel_id{channel_id},
  from_path(move(from_path)),
  kind{kind}
{
  //
}

Cookie::Cookie(Cookie &&other) noexcept :
  channel_id{other.channel_id},
  from_path(move(other.from_path)),
  kind{other.kind}
{
  //
}

void CookieBatch::moved_from(MessageBuffer &messages,
  ChannelID channel_id,
  uint32_t cookie,
  string &&old_path,
  EntryKind kind)
{
  auto existing = from_paths.find(cookie);
  if (existing != from_paths.end()) {
    // Duplicate IN_MOVED_FROM cookie.
    // Resolve the old one as a deletion.
    Cookie dup(move(existing->second));
    messages.deleted(dup.get_channel_id(), dup.move_from_path(), dup.get_kind());
    from_paths.erase(existing);
  }

  Cookie c(channel_id, move(old_path), kind);
  from_paths.emplace(cookie, move(c));
}

unique_ptr<Cookie> CookieBatch::yoink(uint32_t cookie)
{
  auto from = from_paths.find(cookie);
  if (from == from_paths.end()) {
    return unique_ptr<Cookie>(nullptr);
  }

  unique_ptr<Cookie> c(new Cookie(move(from->second)));
  from_paths.erase(from);
  return c;
}

void CookieBatch::flush(MessageBuffer &messages, RecentFileCache &cache)
{
  for (auto &pair : from_paths) {
    Cookie dup(move(pair.second));
    cache.evict(dup.get_from_path());
    messages.deleted(dup.get_channel_id(), dup.move_from_path(), dup.get_kind());
  }
  from_paths.clear();
}

CookieJar::CookieJar(unsigned int max_batches) : batches(max_batches)
{
  //
}

void CookieJar::moved_from(MessageBuffer &messages,
  ChannelID channel_id,
  uint32_t cookie,
  std::string &&old_path,
  EntryKind kind)
{
  if (batches.empty()) return;

  batches.back().moved_from(messages, channel_id, cookie, move(old_path), kind);
}

void CookieJar::moved_to(MessageBuffer &messages,
  ChannelID channel_id,
  uint32_t cookie,
  std::string &&new_path,
  EntryKind kind)
{
  unique_ptr<Cookie> from;
  for (auto &batch : batches) {
    unique_ptr<Cookie> found = batch.yoink(cookie);
    if (found) {
      if (from) {
        // Multiple IN_MOVED_FROM results.
        // Report deletions for all but the most recent.
        messages.deleted(from->get_channel_id(), from->move_from_path(), from->get_kind());
      }

      from = move(found);
    }
  }

  if (!from) {
    // Unmatched IN_MOVED_TO.
    // Resolve it as a creation.
    messages.created(channel_id, move(new_path), kind);
    return;
  }

  if (from->get_channel_id() != channel_id || kinds_are_different(from->get_kind(), kind)) {
    // Existing IN_MOVED_FROM with this cookie does not match.
    // Resolve it as a deletion/creation pair.
    messages.deleted(from->get_channel_id(), from->move_from_path(), from->get_kind());
    messages.created(channel_id, move(new_path), kind);
    return;
  }

  messages.renamed(channel_id, from->move_from_path(), move(new_path), kind);
}

void CookieJar::flush_oldest_batch(MessageBuffer &messages, RecentFileCache &cache)
{
  if (batches.empty()) return;

  batches.front().flush(messages, cache);
  batches.pop_front();
  batches.emplace_back();
}
