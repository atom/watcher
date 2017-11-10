#include <CoreServices/CoreServices.h>
#include <functional>
#include <iomanip>
#include <utility>

#include "../../log.h"
#include "helper.h"

void SourceFnRegistry::callback(void *info)
{
  auto it_ptr = static_cast<Iter *>(info);
  auto it = *it_ptr;
  FnRegistryAction action = it->fn();
  if (action == FN_DISPOSE) {
    it->registry->fns.erase(it);
    delete it_ptr;
  }
}

void TimerFnRegistry::callback(CFRunLoopTimerRef timer, void *info)
{
  auto it_ptr = reinterpret_cast<Iter *>(info);
  auto it = *it_ptr;
  FnRegistryAction action = it->fn(timer);
  if (action == FN_DISPOSE) {
    it->registry->fns.erase(it);
    delete it_ptr;
  }
}

void EventStreamFnRegistry::callback(ConstFSEventStreamRef ref,
  void *info,
  size_t num_events,
  void *event_paths,
  const FSEventStreamEventFlags *event_flags,
  const FSEventStreamEventId *event_ids)
{
  auto it_ptr = reinterpret_cast<Iter *>(info);
  auto it = *it_ptr;
  FnRegistryAction action = it->fn(ref, num_events, event_paths, event_flags, event_ids);
  if (action == FN_DISPOSE) {
    it->registry->fns.erase(it);
    delete it_ptr;
  }
}
