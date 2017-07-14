let sfw = null
try {
  sfw = require('../build/Release/sfw.node')
} catch (err) {
  sfw = require('../build/Debug/sfw.node')
}

class NativeWatcher {
  constructor (rootPath) {
    this.rootPath = rootPath
  }

  unwatch () {
    return new Promise((resolve, reject) => {
      sfw.unwatch(this.rootPath, err => (err ? reject(err) : resolve()))
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
      sfw.watch(rootPath, err => {
        if (err) {
          reject(err)
          return
        }

        resolve(new NativeWatcher(rootPath))
      })
    })
  }
}
