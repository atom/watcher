const { PathWatcherManager } = require('./path-watcher-manager')
const { configure, status, DISABLE, STDERR, STDOUT } = require('./binding')

// Extended: Invoke a callback with each filesystem event that occurs beneath a specified path.
//
// watchPath handles the efficient re-use of operating system resources across living watchers. Watching the same path
// more than once, or the child of a watched path, will re-use the existing native watcher.
//
// * `rootPath` {String} specifies the absolute path to the root of the filesystem content to watch.
// * `options` Control the watcher's behavior.
// * `eventCallback` {Function} or other callable to be called each time a batch of filesystem events is observed.
//    * `events` {Array} of objects that describe the events that have occurred.
//      * `action` {String} describing the filesystem action that occurred. One of `"created"`, `"modified"`,
//        `"deleted"`, or `"renamed"`.
//      * `kind` {String} distinguishing the type of filesystem entry that was acted upon, when available. One of
//        `"file"`, `"directory"`, or `"unknown"`.
//      * `path` {String} containing the absolute path to the filesystem entry that was acted upon.
//      * `oldPath` For rename events, {String} containing the filesystem entry's former absolute path.
//
// Returns a {Promise} that will resolve to a {PathWatcher} once it has started. Note that every {PathWatcher}
// is a {Disposable}, so they can be managed by a {CompositeDisposable} if desired.
//
// ```js
// const {watchPath} = require('@atom/watcher')
//
// const disposable = await watchPath('/var/log', {}, events => {
//   console.log(`Received batch of ${events.length} events.`)
//   for (const event of events) {
//     // "created", "modified", "deleted", "renamed"
//     console.log(`Event action: ${event.action}`)
//
//     // absolute path to the filesystem entry that was touched
//     console.log(`Event path: ${event.path}`)
//
//     // "file", "directory", or "unknown"
//     console.log(`Event kind: ${event.kind}`)
//
//     if (event.action === 'renamed') {
//       console.log(`.. renamed from: ${event.oldPath}`)
//     }
//   }
// })
//
//  // Immediately stop receiving filesystem events. If this is the last
//  // watcher, asynchronously release any OS resources required to
//  // subscribe to these events.
//  disposable.dispose()
// ```
function watchPath (rootPath, options, eventCallback) {
  const watcher = PathWatcherManager.instance().createWatcher(rootPath, options, eventCallback)
  return watcher.getStartPromise().then(() => watcher)
}

// Private: Return a Promise that resolves when all {NativeWatcher} instances associated with a FileSystemManager
// have stopped listening. This is useful for `afterEach()` blocks in unit tests.
function stopAllWatchers () {
  return PathWatcherManager.instance().stopAllWatchers()
}

function getRegistry () {
  return PathWatcherManager.instance().getRegistry()
}

// Private: Show the currently active native watchers.
function printWatchers () {
  return PathWatcherManager.instance().print()
}

module.exports = {
  watchPath,
  stopAllWatchers,
  getRegistry,
  printWatchers,
  configure,
  status,
  DISABLE,
  STDERR,
  STDOUT
}
