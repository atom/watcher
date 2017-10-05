// Shared helper functions

const path = require('path')
const fs = require('fs-extra')

const watcher = require('../lib')
const {DISABLE} = watcher

async function prepareFixtureDir () {
  const rootDir = path.join(__dirname, 'fixture')
  const fixtureDir = await fs.mkdtemp(path.join(rootDir, 'watched-'))
  const watchDir = path.join(fixtureDir, 'root')
  await fs.mkdirs(watchDir)

  const mainLogFile = path.join(fixtureDir, 'main.test.log')
  const workerLogFile = path.join(fixtureDir, 'worker.test.log')
  const pollingLogFile = path.join(fixtureDir, 'polling.test.log')

  await Promise.all([
    [mainLogFile, workerLogFile, pollingLogFile].map(fname => fs.unlink(fname, {encoding: 'utf8'}).catch(() => ''))
  ])

  return {fixtureDir, watchDir, mainLogFile, workerLogFile, pollingLogFile}
}

async function reportLogs (currentTest, mainLogFile, workerLogFile, pollingLogFile) {
  if (process.platform === 'win32') {
    await watcher.configure({mainLog: DISABLE, workerLog: DISABLE})
  }

  if (currentTest.state === 'failed' || process.env.VERBOSE) {
    const [mainLog, workerLog, pollingLog] = await Promise.all(
      [mainLogFile, workerLogFile, pollingLogFile].map(fname => fs.readFile(fname, {encoding: 'utf8'}).catch(() => ''))
    )

    console.log(`>>> main log ${mainLogFile}:\n${mainLog}\n<<<\n`)
    console.log(`>>> worker log ${workerLogFile}:\n${workerLog}\n<<<\n`)
    console.log(`>>> polling log ${pollingLogFile}:\n${pollingLog}\n<<<\n`)
  }
}

function cleanupFixtureDir (fixtureDir) {
  return fs.remove(fixtureDir, {maxBusyTries: 1})
    .catch(err => console.warn('Unable to delete fixture directory', err))
}

module.exports = {prepareFixtureDir, cleanupFixtureDir, reportLogs}
