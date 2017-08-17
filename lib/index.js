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
  [1, 'directory']
])

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
    return new Promise((resolve, reject) => {
      watcher.configure(options, err => (err ? reject(err) : resolve(err)))
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
  }
}
