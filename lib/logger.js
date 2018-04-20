const util = require('util');
const fs = require('fs');

class StreamLogger {
  constructor(stream) {
    this.stream = stream
  }

  log(...args) {
    this.stream.write(util.format(...args) + '\n')
  }
}

const nullLogger = {
  log() {},
}

let activeLogger = null

function disable() {
  activeLogger = nullLogger
}

function toStdout() {
  activeLogger = new StreamLogger(process.stdout)
}

function toStderr() {
  activeLogger = new StreamLogger(process.stderr)
}

function toFile(filePath) {
  const stream = fs.createWriteStream(filePath, {defaultEncoding: 'utf8', flags: 'a'})
  activeLogger = new StreamLogger(stream)
}

function fromEnv(value) {
  if (!value) {
    return disable();
  } else if (value === 'stdout') {
    return toStdout();
  } else if (value === 'stderr') {
    return toStderr();
  } else {
    return toFile(value);
  }
}

function getActiveLogger() {
  if (activeLogger === null) {
    fromEnv(process.env.WATCHER_LOG_JS)
  }
  return activeLogger;
}

module.exports = {
  disable,
  toStdout,
  toStderr,
  toFile,
  fromEnv,
  log(...args) { return getActiveLogger().log(...args) }
}
