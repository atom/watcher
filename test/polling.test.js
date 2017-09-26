const watcher = require('../lib')

const {prepareFixtureDir, reportLogs, cleanupFixtureDir} = require('./helper')

describe('polling', function () {
  let watchDir, fixtureDir, mainLogFile, workerLogFile, sub

  beforeEach(async function () {
    ({watchDir, fixtureDir, mainLogFile, workerLogFile} = await prepareFixtureDir())

    await watcher.configure({
      mainLog: mainLogFile,
      workerLog: workerLogFile
    })
  })

  afterEach(async function () {
    if (sub) await sub.unwatch()
    await reportLogs(this.currentTest, mainLogFile, workerLogFile)
    await cleanupFixtureDir(fixtureDir)
  })

  describe('thread state', function () {
    it('does not run the polling thread while no paths are being polled', function () {
      const status = watcher.status()
      assert.isFalse(status.pollingThreadActive)
    })

    it('runs the polling thread when polling a directory for changes', async function () {
      sub = await watcher.watch(watchDir, {poll: true}, () => {})
      assert.isTrue(watcher.status().pollingThreadActive)

      await sub.unwatch()
      assert.isFalse(watcher.status().pollingThreadActive)
    })
  })
})
