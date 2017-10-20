# MacOS

On MacOS, @atom/watcher uses the [FSEvents API](https://developer.apple.com/library/content/documentation/Darwin/Conceptual/FSEvents_ProgGuide/UsingtheFSEventsFramework/UsingtheFSEventsFramework.html) from Core Foundation. Each registered watch root creates a new `FSEventStream` which is scheduled on the [`CFRunLoop`](https://developer.apple.com/documentation/corefoundation/cfrunloop-rht) associated with the single worker thread. Out-of-band commands are delivered to the worker thread by signalling a [`CFRunLoopSource`](https://developer.apple.com/documentation/corefoundation/1542679-cfrunloopsourcecreate?language=objc).

## FSEvents oddities

FSEvents coalesces filesystem events that occur at the same path within a few-second window, OR'ing together the bits set by individual actions. For example, if these events are performed in rapid succession:

```sh
mkdir /watchroot/a-directory
touch /watchroot/some-path
echo "contents" > /watchroot/some-path
rm /watchroot/some-path
mv /watchroot/a-directory /watchroot/some-path
```

An FSEventStream callback may receive only these events entries:

```
/watchroot/a-directory =
  kFSEventStreamEventFlagItemCreated |
  kFSEventStreamEventFlagItemRenamed |
  kFSEventStreamEventFlagItemIsDir
/watchroot/some-path =
  kFSEventStreamEventFlagItemCreated |
  kFSEventStreamEventFlagItemModified |
  kFSEventStreamEventFlagItemRemoved |
  kFSEventStreamEventFlagItemRenamed |
  kFSEventStreamEventFlagItemIsFile |
  kFSEventStreamEventFlagItemIsDir
```

Depending on the timing involved and the way that events are batched, sometimes a path's event entry may be delivered multiple times, with new bits OR'd in each time:

```
/watchroot/a-directory =
  kFSEventStreamEventFlagItemCreated |
  kFSEventStreamEventFlagItemIsDir
/watchroot/some-path =
  kFSEventStreamEventFlagItemCreated |
  kFSEventStreamEventFlagItemModified |
  kFSEventStreamEventFlagItemIsFile
/watchroot/some-path =
  kFSEventStreamEventFlagItemCreated |
  kFSEventStreamEventFlagItemModified |
  kFSEventStreamEventFlagItemRemoved |
  kFSEventStreamEventFlagItemIsFile
/watchroot/a-directory =
  kFSEventStreamEventFlagItemCreated |
  kFSEventStreamEventFlagItemRenamed |
  kFSEventStreamEventFlagItemIsDir
/watchroot/some-path =
  kFSEventStreamEventFlagItemCreated |
  kFSEventStreamEventFlagItemModified |
  kFSEventStreamEventFlagItemRemoved |
  kFSEventStreamEventFlagItemRenamed |
  kFSEventStreamEventFlagItemIsFile |
  kFSEventStreamEventFlagItemIsDir
```

To disentangle this, @atom/watcher combines information from:

* An `lstat()` call performed the last time an event occurred at this path, if an event has occurred at this path recently;
* The bits that are set in each event;
* A recent `lstat()` call that was performed during this event batch.

By combining these data points, @atom/watcher heuristically deduces what actions must have occurred on the filesystem to result in the observed sequence of events.

FSEvents provides no mechanism to associate the old and new sides of a rename event. It only produces an event at the old and new paths with `kFSEventStreamEventFlagItemRenamed` bit set. This bit is set regardless of whether or not the source or destination are both watched and are not guaranteed to arrive with deterministic order or timing. @atom/watcher uses a cache (storing a maximum of 4k entries) of recently observed `lstat()` results to correlate rename events by inode. If half of a rename event is unpaired after 50ms, it is emitted as a `"create"` or `"delete"` instead.

## Known platform limits

After roughly 450 event streams have been created and attached, `FSEventStreamStart()` will fail by returning false. When this is detected @atom/watcher falls back to polling.
