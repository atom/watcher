const binding = require('./binding')
const { Emitter, CompositeDisposable, Disposable } = require('event-kit')
const { log } = require('./logger')

const ACTIONS = new Map([
  [0, 'created'],
  [1, 'deleted'],
  [2, 'modified'],
  [3, 'renamed']
])

const ENTRIES = new Map([
  [0, 'file'],
  [1, 'directory'],
  [2, 'symlink'],
  [3, 'unknown']
])

// Private: Possible states of a {NativeWatcher}.
const STOPPED = Symbol('stopped')
const STARTING = Symbol('starting')
const RUNNING = Symbol('running')
const STOPPING = Symbol('stopping')

// Private: Interface with and normalize events from a native OS filesystem watcher.
class NativeWatcher {
  // Private: Initialize a native watcher on a path.
  //
  // Events will not be produced until {start()} is called.
  constructor (normalizedPath, options) {
    this.normalizedPath = normalizedPath
    this.options = options
    this.emitter = new Emitter()
    this.subs = new CompositeDisposable()

    this.channel = null
    this.state = STOPPED

    this.onEvents = this.onEvents.bind(this)
    this.onError = this.onError.bind(this)

    log('create NativeWatcher %s with options %j.', this, this.options)
  }

  // Private: Begin watching for filesystem events.
  //
  // Has no effect if the watcher has already been started.
  async start () {
    if (this.state === STARTING) {
      log('NativeWatcher %s is already starting.', this)
      await new Promise(resolve => this.emitter.once('did-start', resolve))
      return
    }

    if (this.state === RUNNING || this.state === STOPPING) {
      log('NativeWatcher %s is running or stopping.', this)
      return
    }

    log('Starting NativeWatcher %s.', this)
    this.state = STARTING

    this.channel = await new Promise((resolve, reject) => {
      binding.watch(this.normalizedPath, this.options, (err, channel) => {
        if (err) {
          reject(err)
          return
        }

        resolve(channel)
      }, this.onEvents)
    })
    log('NativeWatcher %s assigned channel %d.', this, this.channel)

    this.state = RUNNING
    this.emitter.emit('did-start')
  }

  // Private: Return true if the underlying watcher is actively listening for filesystem events.
  isRunning () {
    return this.state === RUNNING
  }

  // Private: Access this {NativeWatcher}. For compatibility with {PathWatcher}.
  getNativeWatcher () {
    return this
  }

  // Private: Register a callback to be invoked when the filesystem watcher has been initialized.
  //
  // Returns: A {Disposable} to revoke the subscription.
  onDidStart (callback) {
    return this.emitter.on('did-start', callback)
  }

  // Private: Register a callback to be invoked with normalized filesystem events as they arrive. Starts the watcher
  // automatically if it is not already running. The watcher will be stopped automatically when all subscribers
  // dispose their subscriptions.
  //
  // Returns: A {Disposable} to revoke the subscription.
  onDidChange (callback) {
    this.start()

    const sub = this.emitter.on('did-change', callback)
    return new Disposable(() => {
      sub.dispose()
      if (this.emitter.listenerCountForEventName('did-change') === 0) {
        log('Last subscriber disposed on NativeWatcher %s.', this)
        this.stop()
      }
    })
  }

  // Private: Register a callback to be invoked when a {PathWatcher} should attach to a different {NativeWatcher}.
  //
  // Returns: A {Disposable} to revoke the subscription.
  onShouldDetach (callback) {
    return this.emitter.on('should-detach', callback)
  }

  // Private: Register a callback to be invoked when a {NativeWatcher} is about to be stopped.
  //
  // Returns: A {Disposable} to revoke the subscription.
  onWillStop (callback) {
    return this.emitter.on('will-stop', callback)
  }

  // Private: Register a callback to be invoked when the filesystem watcher has been stopped.
  //
  // Returns: A {Disposable} to revoke the subscription.
  onDidStop (callback) {
    return this.emitter.on('did-stop', callback)
  }

  // Private: Register a callback to be invoked with any errors reported from the watcher.
  //
  // Returns: A {Disposable} to revoke the subscription.
  onDidError (callback) {
    return this.emitter.on('did-error', callback)
  }

  // Private: Broadcast an `onShouldDetach` event to prompt any {PathWatcher} instances bound here to attach to a new
  // {NativeWatcher} instead.
  //
  // * `replacement` the new {NativeWatcher} instance that a live {PathWatcher} instance should reattach to instead.
  // * `watchedPath` absolute path watched by the new {NativeWatcher}.
  // * `options` used to create the new {NativeWatcher}.
  reattachTo (replacement, watchedPath, options) {
    this.emitter.emit('should-detach', { replacement, watchedPath, options })
  }

  // Private: Stop the native watcher and release any operating system resources associated with it.
  //
  // * `split` {Boolean} that determines whether or not this native watcher may split into multiple
  //   child watchers on removal.
  //
  // Has no effect if the watcher is not running.
  async stop (split = true) {
    if (this.state === STOPPING) {
      log('NativeWatcher %s is already stopping.', this)
      await new Promise(resolve => this.emitter.once('did-stop', resolve))
      return
    }

    if (this.state === STARTING) {
      log('NativeWatcher %s is still starting.', this)
      await new Promise(resolve => this.emitter.once('did-start', resolve))
    }

    if (this.state === STOPPED) {
      log('NativeWatcher %s has already stopped.', this)
      return
    }

    if (!this.channel) {
      throw new Error('Cannot stop a watcher with no channel')
    }

    log('Stopping NativeWatcher %s with split %s.', this, split)
    this.state = STOPPING
    this.emitter.emit('will-stop', split)

    await new Promise((resolve, reject) => {
      binding.unwatch(this.channel, err => (err ? reject(err) : resolve()))
    })
    this.channel = null
    this.state = STOPPED
    log('NativeWatcher %s has been stopped.', this)

    this.emitter.emit('did-stop')
  }

  // Private: Callback function invoked by the native watcher when a debounced group of filesystem events arrive.
  // Normalize and re-broadcast them to any subscribers.
  //
  // * `events` An Array of filesystem events.
  onEvents (err, events) {
    if (err) {
      return this.onError(err)
    }

    const translated = events.map(event => {
      const n = {
        action: ACTIONS.get(event.action),
        kind: ENTRIES.get(event.kind),
        path: event.path
      }

      if (event.oldPath !== '') n.oldPath = event.oldPath

      return n
    })

    this.emitter.emit('did-change', translated)
  }

  // Private: Callback function invoked by the native watcher when an error occurs.
  //
  // * `err` The native filesystem error.
  onError (err) {
    this.emitter.emit('did-error', err)
  }

  // Extended: Report this watcher's path and state.
  toString () {
    return `[NativeWatcher path=${this.normalizedPath} state=${this.state.toString()}]`
  }
}

module.exports = { NativeWatcher }
