#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include "../../helper/macos/helper.h"
#include "../../message.h"
#include <CoreServices/CoreServices.h>

class Subscription
{
public:
  Subscription(ChannelID channel_id, bool recursive, RefHolder<FSEventStreamRef> &&event_stream);

  Subscription(Subscription &&original);

  ~Subscription();

  const ChannelID &get_channel_id() { return channel_id; }

  const bool &get_recursive() { return recursive; }

  const RefHolder<FSEventStreamRef> &get_event_stream() { return event_stream; }

  Subscription(const Subscription &) = delete;
  Subscription &operator=(const Subscription &) = delete;
  Subscription &operator=(Subscription &&) = delete;

private:
  ChannelID channel_id;
  bool recursive;
  RefHolder<FSEventStreamRef> event_stream;
};

#endif
