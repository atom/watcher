const path = require('path')
const { log } = require('./logger')
const { Tree } = require('./registry/tree')

// Private: Track the directories being monitored by native filesystem watchers. Minimize the number of native watchers
// allocated to receive events for a desired set of directories by:
//
// 1. Subscribing to the same underlying {NativeWatcher} when watching the same directory multiple times.
// 2. Subscribing to an existing {NativeWatcher} on a parent of a desired directory.
// 3. Replacing multiple {NativeWatcher} instances on child directories with a single new {NativeWatcher} on the
//    parent.
class NativeWatcherRegistry {
  // Private: Instantiate an empty registry.
  //
  // * `createNative` {Function} that will be called with a normalized filesystem path to create a new native
  //   filesystem watcher.
  constructor (createNative) {
    this.tree = new Tree([], createNative)
  }

  // Private: Attach a watcher to a directory, assigning it a {NativeWatcher}. If a suitable {NativeWatcher} already
  // exists, it will be attached to the new {PathWatcher} with an appropriate subpath configuration. Otherwise, the
  // `createWatcher` callback will be invoked to create a new {NativeWatcher}, which will be registered in the tree
  // and attached to the watcher.
  //
  // If any pre-existing child watchers are removed as a result of this operation, {NativeWatcher.onWillReattach} will
  // be broadcast on each with the new parent watcher as an event payload to give child watchers a chance to attach to
  // the new watcher.
  //
  // * `watcher` an unattached {PathWatcher}.
  async attach (watcher) {
    log('attaching watcher %s to native registry.', watcher)
    const normalizedDirectory = await watcher.getNormalizedPathPromise()
    const pathSegments = normalizedDirectory.split(path.sep).filter(segment => segment.length > 0)

    log('adding watcher %s to tree.', watcher)
    this.tree.add(pathSegments, watcher.getOptions(), (native, nativePath, options) => {
      watcher.attachToNative(native, nativePath, options)
    })
    log('watcher %s added. tree state:\n%s', watcher, this.print())
  }

  // Private: Generate a visual representation of the currently active watchers managed by this
  // registry.
  //
  // Returns a {String} showing the tree structure.
  print () {
    return this.tree.print()
  }
}

module.exports = { NativeWatcherRegistry }
