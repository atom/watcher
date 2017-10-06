// Shared helper functions

const path = require('path')
const fs = require('fs-extra')

const watcher = require('../lib')
const {DISABLE} = watcher

class Fixture {
  constructor () {
    this.subs = []
  }

  async before () {
    const rootDir = path.join(__dirname, 'fixture')
    this.fixtureDir = await fs.mkdtemp(path.join(rootDir, 'watched-'))
    this.watchDir = path.join(this.fixtureDir, 'root')

    this.mainLogFile = path.join(this.fixtureDir, 'main.test.log')
    this.workerLogFile = path.join(this.fixtureDir, 'worker.test.log')
    this.pollingLogFile = path.join(this.fixtureDir, 'polling.test.log')

    await fs.mkdirs(this.watchDir)
    return Promise.all([
      [this.mainLogFile, this.workerLogFile, this.pollingLogFile].map(fname => {
        fs.unlink(fname, {encoding: 'utf8'}).catch(() => '')
      })
    ])
  }

  log () {
    return watcher.configure({
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
    const sub = await watcher.watch(watchRoot, options, callback)
    this.subs.push(sub)
    return sub
  }

  async after (currentTest) {
    await Promise.all(this.subs.map(sub => sub.unwatch()))

    if (process.platform === 'win32') {
      await watcher.configure({mainLog: DISABLE, workerLog: DISABLE, pollingLog: DISABLE})
    }

    if (currentTest.state === 'failed' || process.env.VERBOSE) {
      const [mainLog, workerLog, pollingLog] = await Promise.all(
        [this.mainLogFile, this.workerLogFile, this.pollingLogFile].map(fname => {
          return fs.readFile(fname, {encoding: 'utf8'}).catch(() => '')
        })
      )

      console.log(`>>> main log ${this.mainLogFile}:\n${mainLog}\n<<<\n`)
      console.log(`>>> worker log ${this.workerLogFile}:\n${workerLog}\n<<<\n`)
      console.log(`>>> polling log ${this.pollingLogFile}:\n${pollingLog}\n<<<\n`)
    }

    await fs.remove(this.fixtureDir, {maxBusyTries: 1})
      .catch(err => console.warn('Unable to delete fixture directory', err))
  }
}

module.exports = {Fixture}
