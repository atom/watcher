const watcher = require('../lib')

const {prepareFixtureDir, reportLogs, cleanupFixtureDir} = require('./helper')

describe('polling', function () {
  let watchDir, fixtureDir, mainLogFile, workerLogFile, pollingLogFile, sub

  beforeEach(async function () {
    ({watchDir, fixtureDir, mainLogFile, workerLogFile, pollingLogFile} = await prepareFixtureDir())

    await watcher.configure({
      mainLog: mainLogFile,
      workerLog: workerLogFile,
      pollingLog: pollingLogFile
    })
  })

  afterEach(async function () {
    if (sub) await sub.unwatch()
    await reportLogs(this.currentTest, mainLogFile, workerLogFile, pollingLogFile)
    await cleanupFixtureDir(fixtureDir)
  })

  describe('thread state', function () {
    it('does not run the polling thread while no paths are being polled', function () {
      assert.equal(watcher.status().pollingThreadState, 'stopped')
    })

    it('runs the polling thread when polling a directory for changes', async function () {
      sub = await watcher.watch(watchDir, {poll: true}, () => {})
      assert.equal(watcher.status().pollingThreadState, 'running')

      await sub.unwatch()
      await until(() => watcher.status().pollingThreadState === 'stopped')
    })
  })
})
