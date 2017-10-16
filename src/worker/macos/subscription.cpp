#include <CoreServices/CoreServices.h>
#include <utility>

#include "../../helper/macos/helper.h"
#include "../../message.h"
#include "subscription.h"

using std::move;

Subscription::Subscription(ChannelID channel_id, bool recursive, RefHolder<FSEventStreamRef> &&event_stream) :
  channel_id{channel_id},
  recursive{recursive},
  event_stream{move(event_stream)}
{
  //
}

Subscription::Subscription(Subscription &&original) :
  channel_id{original.channel_id},
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
