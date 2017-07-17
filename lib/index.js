let sfw = null
try {
  sfw = require('../build/Release/sfw.node')
} catch (err) {
  sfw = require('../build/Debug/sfw.node')
}

class NativeWatcher {
  constructor (channel) {
    this.channel = channel
  }

  unwatch () {
    return new Promise((resolve, reject) => {
      sfw.unwatch(this.channel, err => (err ? reject(err) : resolve()))
    })
  }
}

module.exports = {
  configure: function (options) {
    return new Promise((resolve, reject) => {
      sfw.configure(options, err => (err ? reject(err) : resolve(err)))
    })
  },

  watch: function (rootPath, eventCallback) {
    return new Promise((resolve, reject) => {
      sfw.watch(rootPath, (err, channel) => {
        if (err) {
          reject(err)
          return
        }

        resolve(new NativeWatcher(channel))
      }, eventCallback || (() => {}))
    })
  }
}
