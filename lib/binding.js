const logger = require('./logger');

let watcher = null
function getWatcher () {
  if (!watcher) {
    try {
      watcher = require('../build/Release/watcher.node')
    } catch (err) {
      watcher = require('../build/Debug/watcher.node')
    }
  }
  return watcher
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

function jsLogOption (value) {
  if (value === undefined) return

  if (value === DISABLE) {
    logger.disable()
    return
  }

  if (value === STDERR) {
    logger.toStderr()
    return
  }

  if (value === STDOUT) {
    logger.toStdout()
    return
  }

  if (typeof value === 'string' || value instanceof String) {
    logger.toFile(value)
    return
  }

  throw new Error('option jsLog must be DISABLE, STDERR, STDOUT, or a filename')
}

function configure (options) {
  if (!options) {
    return Promise.reject(new Error('configure() requires an option object'))
  }

  const normalized = {}

  logOption('mainLog', options, normalized)
  logOption('workerLog', options, normalized)
  logOption('pollingLog', options, normalized)
  jsLogOption(options.jsLog)

  if (options.workerCacheSize) normalized.workerCacheSize = options.workerCacheSize
  if (options.pollingThrottle) normalized.pollingThrottle = options.pollingThrottle
  if (options.pollingInterval) normalized.pollingInterval = options.pollingInterval

  return new Promise((resolve, reject) => {
    getWatcher().configure(normalized, err => (err ? reject(err) : resolve()))
  })
}

function status () {
  return new Promise((resolve, reject) => {
    getWatcher().status((err, st) => {
      if (err) { reject(err) } else { resolve(st) }
    })
  })
}

function lazy (key) {
  return function (...args) {
    return getWatcher()[key](...args)
  }
}

module.exports = {
  watch: lazy('watch'),
  unwatch: lazy('unwatch'),
  configure,
  status,

  DISABLE,
  STDERR,
  STDOUT
}
