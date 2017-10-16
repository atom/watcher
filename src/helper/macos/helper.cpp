#include <CoreServices/CoreServices.h>
#include <functional>
#include <iomanip>
#include <utility>

#include "../../log.h"
#include "helper.h"

void SourceFnRegistry::callback(void *info)
{
  auto entry = reinterpret_cast<Entry *>(info);
  FnRegistryAction action = entry->fn();
  if (action == FN_DISPOSE) {
    entry->registry->fns.erase_after(entry->before);
  }
}

void TimerFnRegistry::callback(CFRunLoopTimerRef timer, void *info)
{
  auto entry = reinterpret_cast<Entry *>(info);
  FnRegistryAction action = entry->fn(timer);
  if (action == FN_DISPOSE) {
    entry->registry->fns.erase_after(entry->before);
  }
}

void EventStreamFnRegistry::callback(ConstFSEventStreamRef ref,
  void *info,
  size_t num_events,
  void *event_paths,
  const FSEventStreamEventFlags *event_flags,
  const FSEventStreamEventId *event_ids)
{
  auto entry = reinterpret_cast<Entry *>(info);
  FnRegistryAction action = entry->fn(ref, num_events, event_paths, event_flags, event_ids);
  if (action == FN_DISPOSE) {
    entry->registry->fns.erase_after(entry->before);
  }
}
