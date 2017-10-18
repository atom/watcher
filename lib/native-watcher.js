let watcher = null
try {
  watcher = require('../build/Release/watcher.node')
} catch (err) {
  watcher = require('../build/Debug/watcher.node')
}
const {Emitter, CompositeDisposable, Disposable} = require('event-kit')

const ACTIONS = new Map([
  [0, 'created'],
  [1, 'deleted'],
  [2, 'modified'],
  [3, 'renamed']
])

const ENTRIES = new Map([
  [0, 'file'],
  [1, 'directory'],
  [2, 'unknown']
])

// Private: Logging mode constants
const DISABLE = Symbol('disable')
const STDERR = Symbol('stderr')
const STDOUT = Symbol('stdout')

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
  }

  // Private: Begin watching for filesystem events.
  //
  // Has no effect if the watcher has already been started.
  async start () {
    if (this.state === STARTING) {
      await new Promise(resolve => this.emitter.once('did-start', resolve))
      return
    }

    if (this.state !== STOPPED) {
      return
    }

    this.state = STARTING

    this.channel = await new Promise((resolve, reject) => {
      watcher.watch(this.normalizedPath, this.options, (err, channel) => {
        if (err) {
          reject(err)
          return
        }

        resolve(channel)
      }, this.onEvents)
    })

    this.state = RUNNING
    this.emitter.emit('did-start')
  }

  // Private: Return true if the underlying watcher is actively listening for filesystem events.
  isRunning () {
    return this.state === RUNNING
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
        this.stop()
      }
    })
  }

  // Private: Register a callback to be invoked when a {Watcher} should attach to a different {NativeWatcher}.
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

  // Private: Broadcast an `onShouldDetach` event to prompt any {Watcher} instances bound here to attach to a new
  // {NativeWatcher} instead.
  //
  // * `replacement` the new {NativeWatcher} instance that a live {Watcher} instance should reattach to instead.
  // * `watchedPath` absolute path watched by the new {NativeWatcher}.
  reattachTo (replacement, watchedPath) {
    this.emitter.emit('should-detach', {replacement, watchedPath})
  }

  // Private: Stop the native watcher and release any operating system resources associated with it.
  //
  // Has no effect if the watcher is not running.
  async stop () {
    if (this.state === STOPPING) {
      await new Promise(resolve => this.emitter.once('did-stop', resolve))
      return
    }

    if (this.state !== RUNNING || !this.channel) {
      return
    }
    this.state = STOPPING
    this.emitter.emit('will-stop')

    await new Promise((resolve, reject) => {
      watcher.unwatch(this.channel, err => (err ? reject(err) : resolve()))
    })
    this.channel = null
    this.state = STOPPED

    this.emitter.emit('did-stop')
  }

  // Private: Detach any event subscribers.
  dispose () {
    this.emitter.dispose()
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
}

function logOption (baseName, options, normalized) {
  const value = options[baseName]

  if (value === undefined) return

  if (value === DISABLE) {
    normalized[`${baseName}Disable`] = true
    return
  }

  if (value === STDERR) {
    normalized[`${baseName}Stderr`] = true
    return
  }

  if (value === STDOUT) {
    normalized[`${baseName}Stdout`] = true
    return
  }

  if (typeof value === 'string' || value instanceof String) {
    normalized[`${baseName}File`] = value
    return
  }

  throw new Error(`option ${baseName} must be DISABLE, STDERR, STDOUT, or a filename`)
}

module.exports = {
  configure: function (options) {
    if (!options) {
      return Promise.reject(new Error('configure() requires an option object'))
    }

    const normalized = {}

    logOption('mainLog', options, normalized)
    logOption('workerLog', options, normalized)
    logOption('pollingLog', options, normalized)

    if (options.pollingThrottle) normalized.pollingThrottle = options.pollingThrottle
    if (options.pollingInterval) normalized.pollingInterval = options.pollingInterval

    return new Promise((resolve, reject) => {
      watcher.configure(normalized, err => (err ? reject(err) : resolve(err)))
    })
  },

  status: function () {
    return watcher.status()
  },

  NativeWatcher,
  DISABLE,
  STDERR,
  STDOUT
}
