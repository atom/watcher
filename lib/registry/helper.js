const path = require('path')

// Private: re-join the segments split from an absolute path to form another absolute path.
function absolute (...parts) {
  let candidate = parts.length !== 1 ? path.join(...parts) : parts[0]
  if (process.platform === 'win32' && /^[A-Z]:$/.test(candidate)) candidate += '\\'
  return path.isAbsolute(candidate) ? candidate : path.join(path.sep, candidate)
}

module.exports = { absolute }
