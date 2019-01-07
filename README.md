# Atom Filesystem Watcher

[![Greenkeeper badge](https://badges.greenkeeper.io/atom/watcher.svg)](https://greenkeeper.io/)

| Linux | Windows | MacOS |
|:------|:-------:|------:|
| [![Build Status](https://travis-ci.org/atom/watcher.svg?branch=master)](https://travis-ci.org/atom/watcher) | [![Build status](https://ci.appveyor.com/api/projects/status/xgm4eg6hbj53cpkl/branch/master?svg=true)](https://ci.appveyor.com/project/Atom/watcher/branch/master) | [![CircleCI](https://circleci.com/gh/atom/watcher/tree/master.svg?style=svg)](https://circleci.com/gh/atom/watcher/tree/master) |

**@atom/watcher** is a filesystem watching library for Node.js built to power [Atom](https://atom.io). It prioritizes:

* **High fidelity** to the native filesystem watching system calls provided by each operating system.
* **Graceful degredation** to polling when native events are unavailable.
* Be **gentle on your CPU** even while polling large directory trees by throttling system calls and never using spinlocks. **Respect your RAM** by capping the amount of persisted information. **Stay out of the event loop's way** by delivering filesystem events to JavaScript in batches.
* **Scalability** to thousands of watched root directories and tens of thousands of files per root.
* Comprehensible **diagnostics and logging,** including detailed reporting of operating system errors.

## Installation

@atom/watcher is developed against Node.js 8, but it should work with any Node.js version that supports `async`/`await`. Your system must be able to [build native Node.js modules](https://github.com/nodejs/node-gyp#installation). @atom/watcher supports [MacOS](./docs/macos.md) _(>= MacOS 10.7)_, [Windows](./docs/windows.md) _(>= Windows XP, >= Windows Server 2003)_, and [Linux](./docs/linux.md) _(kernel >= 2.6.27, glibc >= 2.9 :point_right: Ubuntu >= 10.04, Debian >= 6, RHEL >= 6)_.

```bash
$ npm install @atom/watcher
```

## Use

To be notified when any filesystem event occurs beneath `/var/log`:

```js
const watcher = require('@atom/watcher')

// Invoke a callback with each filesystem event that occurs beneath a specified path.
const w = await watcher.watchPath('/var/log', {}, events => {
  console.log(`Received batch of ${events.length} events.`)
  for (const event of events) {
    // "created", "modified", "deleted", "renamed"
    console.log(`Event action: ${event.action}`)

    // Absolute path to the filesystem entry that was touched
    console.log(`Event path: ${event.path}`)

    // "file", "directory", "symlink", or "unknown"
    console.log(`Event entry kind: ${event.kind}`)

    if (event.action === 'renamed') {
      console.log(`.. renamed from: ${event.oldPath}`)
    }
  }
})

// Report errors that occur after the watch root has been started.
w.onDidError(err => {
  console.error(`Something went wrong: ${err}`)
})

// Immediately stop receiving filesystem events. If this is the last watcher on this path, asynchronously release
// any OS resources required to subscribe to these events.
w.dispose()
```

### configure()

Tweak package-global settings. This method may be called even after watchers have been started. The `Promise` it returns resolves when all changed settings have taken effect. All configuration settings are optional. Omitted keys are left unchanged.

```js
const watcher = require('@atom/watcher')

await watcher.configure({
  jsLog: watcher.STDOUT,
  mainLog: watcher.STDERR,
  workerLog: 'worker.log',
  pollingLog: 'polling.log',
  workerCacheSize: 4096,
  pollingThrottle: 1000,
  pollingInterval: 100
})
```

`jsLog` configures the logging of events from the JavaScript layer. It may be one of:

* A `String` specifying a path to log to a file. Be careful that you don't log to a directory that you're watching :innocent:
* The constants `watcher.STDERR` or `watcher.STDOUT` to log to the `node` process' standard error or output streams.
* `watcher.DISABLE` to disable main thread logging. This is the default.

`mainLog` configures the logging of events from the main thread, in line with libuv's event loop. It accepts the same arguments as `jsLog` and also defaults to `watcher.DISABLE`.

`workerLog` configures logging for the worker thread, which is used to interact with native operating system filesystem watching APIs. It accepts the same arguments as `jsLog` and also defaults to `watcher.DISABLE`.

`pollingLog` configures logging for the polling thread, which polls the filesystem when the worker thread is unable to. The polling thread only launches when at least one path needs to be polled. `pollingLog` accepts the same arguments as `jsLog` and also defaults to `watcher.DISABLE`.

`workerCacheSize` controls the number of recently seen stat results are cached within the worker thread. Increasing the cache size will improve the reliability of rename correlation and the entry kinds of deleted entries, but will consume more RAM. The default is `4096`.

`pollingThrottle` controls the rough number of filesystem-touching system calls (`lstat()` and `readdir()`) performed by the polling thread on each polling cycle. Increasing the throttle will improve the timeliness of polled events, especially when watching large directory trees, but will consume more processor cycles and I/O bandwidth. The throttle defaults to `1000`.

`pollingInterval` adjusts the time in milliseconds that the polling thread spends sleeping between polling cycles. Decreasing the interval will improve the timeliness of polled events, but will consume more processor cycles and I/O bandwidth. The interval defaults to `100`.

### watchPath()

Invoke a callback with each batch of filesystem events that occur beneath a specified directory.

```js
const {watchPath} = require('@atom/watcher')
const watcher = await watchPath('/var/log', {recursive: true}, (events) => {
  // ...
})
```

The returned `Promise` resolves to a `PathWatcher` instance when the watcher is fully installed and events are flowing. The `Promise` may reject if the path does not exist, is not a directory, or if an operating system error prevented the watcher from successfully initializing, like a thread failing to launch or memory being exhausted.

The _path_ argument specifies the root directory to watch. This must be an existing directory, but may be relative, contain symlinks, or contain `.` and `..` segments. Multiple independent calls to `watchPath()` may result in `PathWatcher` instances backed by the same native event source or polling root, so it is relatively cheap to create many watchers within the same directory hierarchy across your codebase.

The _options_ argument configures the nature of the watch. Pass `{}` to accept the defaults. Available options are:

* `recursive`: If `true`, filesystem events that occur within subdirectories will be reported as well. If `false`, only changes to immediate children of the provided path will be reported. Defaults to `true`.

The _callback_ argument will be called repeatedly with each batch of filesystem events that are delivered until the [`.dispose() method`](#pathwatcherdispose) is called. Event batches are `Arrays` containing objects with the following keys:

* `action`: a `String` describing the filesystem action that occurred. One of `"created"`, `"modified"`, `"deleted"`, or `"renamed"`.
* `kind`: a `String` distinguishing the type of filesystem entry that was acted upon, if known. One of `"file"`, `"directory"`, `"symlink"`, or `"unknown"`.
* `path`: a `String` containing the absolute path to the filesystem entry that was acted upon. In the event of a rename, this is the _new_ path of the entry.
* `oldPath`: a `String` containing the former absolute path of a renamed filesystem entry. Omitted when action is not `"renamed"`.

The callback _may_ be invoked for filesystem events that occur before the promise is resolved, but it _will_ be invoked for any changes that occur after it resolves. All three arguments are mandatory.

_:spiral_notepad: When writing tests against code that uses `watchPath`, note that you cannot easily assert that an event was **not** delivered. This is especially true on MacOS, where timestamp resolution can cause you to receive events that occurred before you even issued the `watchPath` call!_

### PathWatcher.onDidError()

Invoke a callback with any errors that occur after the watcher has been installed successfully.

```js
const {watchPath} = require('@atom/watcher')
const watcher = await watchPath('/var/log', {}, () => {})

const disposable = watcher.onDidError(err => {
  console.error(err)
})

disposable.dispose()
```

Returns a [`Disposable`](https://github.com/atom/event-kit#consuming-event-subscription-apis) that clients should dispose to release the subscription.

The `callback` argument will be invoked with an `Error` with a stack trace that likely isn't very helpful and a message that hopefully is.

### PathWatcher.dispose()

Release an event subscription. The event callback associated with this `PathWatcher` will not be called after the watcher has been disposed, synchronously. Note that the native resources or polling root used to feed events to this watcher may remain, if another active `PathWatcher` is consuming events from it, and even if they are freed as a result of this disposal they will be freed asynchronously.

```js
const {watchPath} = require('@atom/watcher')
const watcher = await watchPath('/var/log', {}, () => {})

//

watcher.dispose()
```

### Environment variables

Logging may also be configured by setting environment variables. Each of these may be set to an empty string to disable that log, `"stderr"` to output to stderr, `"stdout"` to output to stdout, or a path to write output to a file at that path.

* `WATCHER_LOG_JS`: JavaScript layer logging
* `WATCHER_LOG_MAIN`: Main thread logging
* `WATCHER_LOG_WORKER`: Worker thread logging
* `WATCHER_LOG_POLLING`: Polling thread logging

## CLI

It's possible to call `@atom/watcher` from the command-line, like this:

```sh
$ watcher /path/to/watch
```

Example:

```
created directory: /path/to/watch/foo
deleted directory: /path/to/watch/foo
```

It can be useful for testing the watcher and to describe a scenario when reporting an issue.
