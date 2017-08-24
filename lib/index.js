let watcher = null
try {
  watcher = require('../build/Release/watcher.node')
} catch (err) {
  watcher = require('../build/Debug/watcher.node')
}

const TYPES = new Map([
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

// Logging mode constants
const DISABLE = Symbol('disable')
const STDERR = Symbol('stderr')
const STDOUT = Symbol('stdout')

function logOption(baseName, options, normalized) {
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

class NativeWatcher {
  constructor (channel) {
    this.channel = channel
    this.disposed = false
  }

  unwatch () {
    if (this.disposed) {
      return Promise.resolve()
    }
    this.disposed = true

    return new Promise((resolve, reject) => {
      watcher.unwatch(this.channel, err => (err ? reject(err) : resolve()))
    })
  }
}

module.exports = {
  configure: function (options) {
    const normalized = {}

    logOption('mainLog', options, normalized)
    logOption('workerLog', options, normalized)

    return new Promise((resolve, reject) => {
      watcher.configure(normalized, err => (err ? reject(err) : resolve(err)))
    })
  },

  watch: function (rootPath, eventCallback) {
    const eventMapper = (err, events) => {
      if (err) {
        if (eventCallback) {
          eventCallback(err)
        } else {
          console.error(`[watcher] error: ${err}`)
        }
        return
      }

      if (!eventCallback) return

      const translated = events.map(event => {
        return {
          type: TYPES.get(event.actionType),
          kind: ENTRIES.get(event.entryKind),
          oldPath: event.oldPath,
          newPath: event.newPath
        }
      })

      eventCallback(null, translated)
    }

    return new Promise((resolve, reject) => {
      watcher.watch(rootPath, (err, channel) => {
        if (err) {
          reject(err)
          return
        }

        resolve(new NativeWatcher(channel))
      }, eventMapper)
    })
  },

  status: function () {
    return watcher.status()
  },

  DISABLE,
  STDERR,
  STDOUT
}
