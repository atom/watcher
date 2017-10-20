# Linux

On Linux, @atom/watcher uses [inotify](https://linux.die.net/man/7/inotify). Each watched directory is added to the watch list of a single inotify instance. Out-of-band command processing is triggered by writing to a [pipe](https://linux.die.net/man/2/pipe) shared between the main and worker threads. The worker thread uses [`poll()`](https://linux.die.net/man/2/poll) to wait for either the command trigger or the inotify descriptor to become ready.

## inotify oddities

`inotify` cannot watch directories recursively. To watch directory trees, @atom/watcher creates new watch descriptors for each subdirectory added. There is a race condition here: events triggered between the subdirectory's creation and the worker thread processing it may occur before the subdirectory's watch descriptor is added, and so may be lost.

`inotify` uses a "cookie" field to correlate rename pairs. @atom/watcher attempts to correlate event cookies across consecutive event batches, but if two batches pass without a matching pair, the event is flushed as a creation or deletion instead.

## Known platform limits

Linux systems have a limited number of watch descriptors for each user. This limit is configurable and can vary from distro to distro; on Ubuntu, for example, it defaults to 8192. When watch descriptors are exhausted, @atom/watcher falls back to polling. Note that this can lead to odd situations where a watched subtree is partially watched by inotify and partially polled.
