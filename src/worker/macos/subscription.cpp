#include <CoreServices/CoreServices.h>
#include <utility>

#include "../../helper/macos/helper.h"
#include "../../message.h"
#include "subscription.h"

using std::move;
using std::string;

Subscription::Subscription(ChannelID channel_id,
  bool recursive,
  string &&root,
  RefHolder<FSEventStreamRef> &&event_stream) :
  channel_id{channel_id},
  root{move(root)},
  recursive{recursive},
  event_stream{move(event_stream)}
{
  //
}

Subscription::Subscription(Subscription &&original) noexcept :
  channel_id{original.channel_id},
  root{move(original.root)},
  recursive{original.recursive},
  event_stream{move(original.event_stream)}
{
  //
}

Subscription::~Subscription()
{
  if (event_stream.ok()) {
    FSEventStreamStop(event_stream.get());
    FSEventStreamInvalidate(event_stream.get());
  }
}
