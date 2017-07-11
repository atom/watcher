let sfw = null
try {
  sfw = require('../build/Release/sfw.node')
} catch (err) {
  sfw = require('../build/Debug/sfw.node')
}

module.exports = {
  configure: function (options) {
    return sfw.configure(options)
  }
}
