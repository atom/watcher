let watcher = null
try {
  watcher = require('../build/Release/watcher.node')
} catch (err) {
  watcher = require('../build/Debug/watcher.node')
}

// Private: Logging mode constants
const DISABLE = Symbol('disable')
const STDERR = Symbol('stderr')
const STDOUT = Symbol('stdout')

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

function configure (options) {
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
}

module.exports = {
  watch: watcher.watch,
  unwatch: watcher.unwatch,
  configure,
  status: watcher.status,

  DISABLE,
  STDERR,
  STDOUT
}
