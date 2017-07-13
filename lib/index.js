let sfw = null
try {
  sfw = require('../build/Release/sfw.node')
} catch (err) {
  sfw = require('../build/Debug/sfw.node')
}

module.exports = {
  configure: function (options) {
    return new Promise((resolve, reject) => {
      sfw.configure(options, err => (err ? reject(err) : resolve(err)))
    })
  }
}
