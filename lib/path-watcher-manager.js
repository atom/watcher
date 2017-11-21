const path = require('path')
const {PathWatcher} = require('./path-watcher')
const {NativeWatcher} = require('./native-watcher')
const {NativeWatcherRegistry} = require('./native-watcher-registry')

// Private: Globally tracked state used to de-duplicate related [PathWatchers]{PathWatcher}.
class PathWatcherManager {
  // Private: Access or lazily initialize the singleton manager instance.
  //
  // Returns the one and only {PathWatcherManager}.
  static instance () {
    if (!PathWatcherManager.theManager) {
      PathWatcherManager.theManager = new PathWatcherManager()
    }
    return PathWatcherManager.theManager
  }

  // Private: Initialize global {PathWatcher} state.
  constructor () {
    this.live = new Set()
    this.nativeRegistry = new NativeWatcherRegistry(
      (normalizedPath, options) => {
        const nativeWatcher = new NativeWatcher(normalizedPath, options)

        this.live.add(nativeWatcher)
        const sub = nativeWatcher.onWillStop(() => {
          this.live.delete(nativeWatcher)
          sub.dispose()
        })

        return nativeWatcher
      }
    )
  }

  // Private: Access the {nativeRegistry} for introspection and diagnostics.
  getRegistry () {
    return this.nativeRegistry
  }

  // Private: Create a {PathWatcher} tied to this global state. See {watchPath} for detailed arguments.
  createWatcher (rootPath, options, eventCallback) {
    const watcher = new PathWatcher(this.nativeRegistry, rootPath, options)
    watcher.onDidChange(eventCallback)

    if (process.platform === 'win32' && !options.poll) {
      watcher.getNormalizedPathPromise().then(normalizedPath => {
        let previousPath = normalizedPath
        let currentPath = path.resolve(normalizedPath, '..')
        while (true) {
          const parentWatcher = new PathWatcher(this.nativeRegistry, currentPath, {recursive: false})
          parentWatcher.onDidChange((function (childPath) {
            return events => {
              for (const event of events) {
                if (event.path !== childPath && event.oldPath !== childPath) continue
                if (event.action !== 'deleted' && event.action !== 'renamed') continue

                parentWatcher.dispose()
                eventCallback([{
                  action: 'deleted',
                  kind: 'directory',
                  path: normalizedPath
                }])
                watcher.dispose()
              }
            }
          })(previousPath))

          let nextPath = path.resolve(currentPath, '..')
          if (nextPath !== currentPath) {
            previousPath = currentPath
            currentPath = nextPath
          } else {
            break
          }
        }
      }, () => {})
    }

    return watcher
  }

  // Private: Return a {String} depicting the currently active native watchers.
  print () {
    return this.nativeRegistry.print()
  }

  // Private: Stop all living watchers.
  //
  // Returns a {Promise} that resolves when all native watcher resources are disposed.
  stopAllWatchers () {
    return Promise.all(
      Array.from(this.live, watcher => watcher.stop(false))
    )
  }
}

module.exports = {PathWatcherManager}
