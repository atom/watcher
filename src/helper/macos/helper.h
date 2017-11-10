#ifndef CFREF_H
#define CFREF_H

#include <CoreServices/CoreServices.h>
#include <functional>
#include <list>
#include <utility>

template <class T>
class RefHolder
{
public:
  RefHolder() : ref{nullptr} {};

  explicit RefHolder(T ref) : ref{ref} {};

  RefHolder(RefHolder<T> &&original) noexcept : ref{original.ref} { original.ref = nullptr; };

  ~RefHolder() { clear(); }

  void set_from_create(T ref)
  {
    assert(this->ref == nullptr);
    this->ref = ref;
  }

  T get()
  {
    assert(this->ref != nullptr);
    return ref;
  }

  bool empty() { return ref == nullptr; }

  bool ok() { return ref != nullptr; }

  void set_from_get(T ref)
  {
    if (ref != nullptr) CFRetain(ref);
    set_from_create(ref);
  }

  void clear()
  {
    if (ref != nullptr) CFRelease(ref);
  }

  RefHolder(const RefHolder &) = delete;
  RefHolder &operator=(const RefHolder &) = delete;
  RefHolder &operator=(RefHolder &&) = delete;

protected:
  T ref;
};

// Specialize RefHolder for FSEventStreamRef to use different retain and release functions

template <>
inline void RefHolder<FSEventStreamRef>::set_from_get(FSEventStreamRef ref)
{
  if (ref != nullptr) FSEventStreamRetain(ref);
  set_from_create(ref);
}

template <>
inline void RefHolder<FSEventStreamRef>::clear()
{
  if (ref != nullptr) FSEventStreamRelease(ref);
}

enum FnRegistryAction
{
  FN_KEEP,
  FN_DISPOSE
};

using SourceFn = std::function<FnRegistryAction()>;

using TimerFn = std::function<FnRegistryAction(CFRunLoopTimerRef)>;

using EventStreamFn = std::function<FnRegistryAction(ConstFSEventStreamRef,
  size_t,
  void *,
  const FSEventStreamEventFlags *,
  const FSEventStreamEventId *)>;

template <class FnType, class This>
class FnRegistry
{
public:
  FnRegistry() = default;

  virtual ~FnRegistry() = default;

  void *create_info(FnType &&fn);

  FnRegistry(const FnRegistry &) = delete;
  FnRegistry(FnRegistry &&) = delete;
  FnRegistry &operator=(const FnRegistry &) = delete;
  FnRegistry &operator=(FnRegistry &&) = delete;

protected:
  struct Entry
  {
    Entry(FnType &&fn, This *registry) : fn(std::move(fn)), registry{registry} {}

    ~Entry() = default;

    FnType fn;
    This *registry;

    Entry(const Entry &) = delete;
    Entry(Entry &&) = delete;
    Entry &operator=(const Entry &) = delete;
    Entry &operator=(Entry &&) = delete;
  };

  void *emplace_entry(FnType &&fn_addr);

  std::list<Entry> fns;

  using Iter = typename std::list<Entry>::const_iterator;
};

class SourceFnRegistry : public FnRegistry<SourceFn, SourceFnRegistry>
{
public:
  static void callback(void *info);

  SourceFnRegistry() = default;

  ~SourceFnRegistry() override = default;

  SourceFnRegistry(const SourceFnRegistry &) = delete;
  SourceFnRegistry(SourceFnRegistry &&) = delete;
  SourceFnRegistry &operator=(const SourceFnRegistry &) = delete;
  SourceFnRegistry &operator=(SourceFnRegistry &&) = delete;
};

class TimerFnRegistry : public FnRegistry<TimerFn, TimerFnRegistry>
{
public:
  static void callback(CFRunLoopTimerRef timer, void *info);

  TimerFnRegistry() = default;

  ~TimerFnRegistry() override = default;

  TimerFnRegistry(const TimerFnRegistry &) = delete;
  TimerFnRegistry(TimerFnRegistry &&) = delete;
  TimerFnRegistry &operator=(const TimerFnRegistry &) = delete;
  TimerFnRegistry &operator=(TimerFnRegistry &&) = delete;
};

class EventStreamFnRegistry : public FnRegistry<EventStreamFn, EventStreamFnRegistry>
{
public:
  static void callback(ConstFSEventStreamRef ref,
    void *info,
    size_t num_events,
    void *event_paths,
    const FSEventStreamEventFlags *event_flags,
    const FSEventStreamEventId *event_ids);

  EventStreamFnRegistry() = default;

  ~EventStreamFnRegistry() override = default;

  EventStreamFnRegistry(const EventStreamFnRegistry &) = delete;
  EventStreamFnRegistry(EventStreamFnRegistry &&) = delete;
  EventStreamFnRegistry &operator=(const EventStreamFnRegistry &) = delete;
  EventStreamFnRegistry &operator=(EventStreamFnRegistry &&) = delete;
};

template <class FnType, class This>
void *FnRegistry<FnType, This>::create_info(FnType &&fn)
{
  fns.emplace_front(std::move(fn), static_cast<This *>(this));
  auto *it = new Iter(fns.cbegin());
  return static_cast<void *>(it);
}

#endif
