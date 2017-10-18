const fs = require('fs-extra')

const {Emitter, CompositeDisposable, Disposable} = require('event-kit')

// Extended: Manage a subscription to filesystem events that occur beneath a root directory. Construct these by
// calling `watchPath`.
//
// Multiple PathWatchers may be backed by a single native watcher to conserve operation system resources.
//
// Call {::dispose} to stop receiving events and, if possible, release underlying resources. A PathWatcher may be
// added to a {CompositeDisposable} to manage its lifetime along with other {Disposable} resources like event
// subscriptions.
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
//
// `watchPath` accepts the following arguments:
//
// `rootPath` {String} specifies the absolute path to the root of the filesystem content to watch.
//
// `options` Control the watcher's behavior. Currently a placeholder.
//
// `eventCallback` {Function} to be called each time a batch of filesystem events is observed. Each event object has
// the keys: `action`, a {String} describing the filesystem action that occurred, one of `"created"`, `"modified"`,
// `"deleted"`, or `"renamed"`; `path`, a {String} containing the absolute path to the filesystem entry that was acted
// upon; `kind`, a {String} describing the type of filesystem entry, one of `"file"`, `"directory"`, or `"unknown"`;
// for rename events only, `oldPath`, a {String} containing the filesystem entry's former absolute path.
class PathWatcher {
  // Private: Instantiate a new PathWatcher. Call {watchPath} instead.
  //
  // * `nativeWatcherRegistry` {NativeWatcherRegistry} used to find and consolidate redundant watchers.
  // * `watchedPath` {String} containing the absolute path to the root of the watched filesystem tree.
  // * `options` See {watchPath} for options.
  constructor (nativeWatcherRegistry, watchedPath, options) {
    this.watchedPath = watchedPath
    this.nativeWatcherRegistry = nativeWatcherRegistry

    this.normalizedPath = null
    this.native = null
    this.changeCallbacks = new Map()

    this.normalizedPathPromise = fs.realpath(watchedPath).then(real => {
      this.normalizedPath = real
      return real
    })

    this.attachedPromise = new Promise(resolve => {
      this.resolveAttachedPromise = resolve
    })
    this.startPromise = new Promise(resolve => {
      this.resolveStartPromise = resolve
    })

    this.emitter = new Emitter()
    this.subs = new CompositeDisposable()
  }

  // Private: Return a {Promise} that will resolve with the normalized root path.
  getNormalizedPathPromise () {
    return this.normalizedPathPromise
  }

  // Private: Return a {Promise} that will resolve the first time that this watcher is attached to a native watcher.
  getAttachedPromise () {
    return this.attachedPromise
  }

  // Extended: Return a {Promise} that will resolve when the underlying native watcher is ready to begin sending events.
  // When testing filesystem watchers, it's important to await this promise before making filesystem changes that you
  // intend to assert about because there will be a delay between the instantiation of the watcher and the activation
  // of the underlying OS resources that feed it events.
  //
  // PathWatchers acquired through `watchPath` are already started.
  //
  // ```js
  // const {watchPath} = require('@atom/watcher')
  // const ROOT = path.join(__dirname, 'fixtures')
  // const FILE = path.join(ROOT, 'filename.txt')
  //
  // describe('something', function () {
  //   it("doesn't miss events", async function () {
  //     const watcher = watchPath(ROOT, {}, events => {})
  //     await watcher.getStartPromise()
  //     fs.writeFile(FILE, 'contents\n', err => {
  //       // The watcher is listening and the event should be
  //       // received asynchronously
  //     }
  //   })
  // })
  // ```
  getStartPromise () {
    return this.startPromise
  }

  // Private: Attach another {Function} to be called with each batch of filesystem events. See {watchPath} for the
  // spec of the callback's argument.
  //
  // * `callback` {Function} to be called with each batch of filesystem events.
  //
  // Returns a {Disposable} that will stop the underlying watcher when all callbacks mapped to it have been disposed.
  onDidChange (callback) {
    if (this.native) {
      const sub = this.native.onDidChange(events => this.onNativeEvents(events, callback))
      this.changeCallbacks.set(callback, sub)

      this.native.start()
    } else {
      // Attach to a new native listener and retry
      this.nativeWatcherRegistry.attach(this).then(() => {
        this.onDidChange(callback)
      })
    }

    return new Disposable(() => {
      const sub = this.changeCallbacks.get(callback)
      this.changeCallbacks.delete(callback)
      sub.dispose()
    })
  }

  // Extended: Invoke a {Function} when any errors related to this watcher are reported.
  //
  // * `callback` {Function} to be called when an error occurs.
  //   * `err` An {Error} describing the failure condition.
  //
  // Returns a {Disposable}.
  onDidError (callback) {
    return this.emitter.on('did-error', callback)
  }

  // Private: Wire this watcher to an operating system-level native watcher implementation.
  attachToNative (native) {
    this.subs.dispose()
    this.native = native

    if (native.isRunning()) {
      this.resolveStartPromise()
    } else {
      this.subs.add(native.onDidStart(() => {
        this.resolveStartPromise()
      }))
    }

    // Transfer any native event subscriptions to the new NativeWatcher.
    for (const [callback, formerSub] of this.changeCallbacks) {
      const newSub = native.onDidChange(events => this.onNativeEvents(events, callback))
      this.changeCallbacks.set(callback, newSub)
      formerSub.dispose()
    }

    this.subs.add(native.onDidError(err => {
      this.emitter.emit('did-error', err)
    }))

    this.subs.add(native.onShouldDetach(({replacement, watchedPath}) => {
      if (this.native === native && replacement !== native && this.normalizedPath.startsWith(watchedPath)) {
        this.attachToNative(replacement)
      }
    }))

    this.subs.add(native.onWillStop(() => {
      if (this.native === native) {
        this.subs.dispose()
        this.native = null
      }
    }))

    this.resolveAttachedPromise()
  }

  // Private: Invoked when the attached native watcher creates a batch of native filesystem events. The native watcher's
  // events may include events for paths above this watcher's root path, so filter them to only include the relevant
  // ones, then re-broadcast them to our subscribers.
  onNativeEvents (events, callback) {
    const filtered = events.filter(event => event.path.startsWith(this.normalizedPath))

    if (filtered.length > 0) {
      callback(filtered)
    }
  }

  // Extended: Unsubscribe all subscribers from filesystem events. Native resources will be release asynchronously,
  // but this watcher will stop broadcasting events immediately.
  dispose () {
    for (const sub of this.changeCallbacks.values()) {
      sub.dispose()
    }

    this.emitter.dispose()
    this.subs.dispose()
  }

  // Extended: Print the directory that this watcher is watching.
  toString () {
    let description = `[Watcher path=${this.watchedPath}`
    if (this.normalizedPath && this.normalizedPath !== this.watchedPath) {
      description += ` normalized=${this.normalizedPath}`
    }
    if (this.native) {
      description += ` native=${this.native}`
    } else {
      description += ' unattached'
    }
    description += ']'

    return description
  }
}

module.exports = {PathWatcher}
