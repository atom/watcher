const path = require('path')

// Private: re-join the segments split from an absolute path to form another absolute path.
function absolute (...parts) {
  const candidate = path.join(...parts)
  return path.isAbsolute(candidate) ? candidate : path.join(path.sep, candidate)
}

module.exports = {absolute}
