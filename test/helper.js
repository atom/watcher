// Shared helper functions

const path = require('path')
const fs = require('fs-extra')
const { CompositeDisposable } = require('event-kit')

const { watchPath, configure, DISABLE } = require('../lib')

class Fixture {
  constructor () {
    this.watchers = []
    this.subs = new CompositeDisposable()
    this.createWatcher = watchPath
  }

  async before () {
    const rootDir = path.join(__dirname, 'fixture')
    this.fixtureDir = await fs.mkdtemp(path.join(rootDir, 'watched-'))
    this.watchDir = path.join(this.fixtureDir, 'root')

    this.mainLogFile = path.join(this.fixtureDir, 'logs', 'main.test.log')
    this.workerLogFile = path.join(this.fixtureDir, 'logs', 'worker.test.log')
    this.pollingLogFile = path.join(this.fixtureDir, 'logs', 'polling.test.log')

    await Promise.all([
      fs.mkdirs(this.watchDir),
      fs.mkdirs(path.join(this.fixtureDir, 'logs'))
    ])
    return Promise.all([
      [this.mainLogFile, this.workerLogFile, this.pollingLogFile].map(fname => {
        fs.unlink(fname, { encoding: 'utf8' }).catch(() => '')
      })
    ])
  }

  log () {
    return configure({
      mainLog: this.mainLogFile,
      workerLog: this.workerLogFile,
      pollingLog: this.pollingLogFile
    })
  }

  fixturePath (...subPath) {
    return path.join(this.fixtureDir, ...subPath)
  }

  watchPath (...subPath) {
    return path.join(this.watchDir, ...subPath)
  }

  async watch (subPath, options, callback) {
    const watchRoot = this.watchPath(...subPath)
    const watcher = await watchPath(watchRoot, options, events => callback(null, events))
    this.subs.add(watcher.onDidError(err => callback(err)))

    this.watchers.push(watcher)
    return watcher
  }

  async after (currentTest) {
    this.subs.dispose()
    this.subs = new CompositeDisposable()

    const natives = new Set(this.watchers.map(watcher => watcher.getNativeWatcher()).filter(Boolean))
    await Promise.all(Array.from(natives, native => native.stop(false)))

    if (process.platform === 'win32') {
      await configure({ mainLog: DISABLE, workerLog: DISABLE, pollingLog: DISABLE })
    }

    if (currentTest.state === 'failed' || process.env.VERBOSE) {
      const [mainLog, workerLog, pollingLog] = await Promise.all(
        [this.mainLogFile, this.workerLogFile, this.pollingLogFile].map(fname => {
          return fs.readFile(fname, { encoding: 'utf8' }).catch(() => '')
        })
      )

      console.log(`>>> main log ${this.mainLogFile}:\n${mainLog}\n<<<\n`)
      console.log(`>>> worker log ${this.workerLogFile}:\n${workerLog}\n<<<\n`)
      console.log(`>>> polling log ${this.pollingLogFile}:\n${pollingLog}\n<<<\n`)
    }

    await fs.remove(this.fixtureDir, { maxBusyTries: 1 })
      .catch(err => console.warn('Unable to delete fixture directory', err))
  }
}

module.exports = { Fixture }
