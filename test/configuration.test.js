const fs = require('fs-extra')

const watcher = require('../lib')

const {prepareFixtureDir, reportLogs, cleanupFixtureDir} = require('./helper')

describe('configuration', function () {
  let fixtureDir, watchDir, mainLogFile, workerLogFile, pollingLogFile

  beforeEach(async function () {
    ({fixtureDir, watchDir, mainLogFile, workerLogFile, pollingLogFile} = await prepareFixtureDir())
  })

  afterEach(async function () {
    await reportLogs(this.currentTest, mainLogFile, workerLogFile, pollingLogFile)
    await cleanupFixtureDir(fixtureDir)
  })

  it('validates its arguments', async function () {
    await assert.isRejected(watcher.configure(), /requires an option object/)
  })

  it('configures the main thread logger', async function () {
    await watcher.configure({mainLog: mainLogFile})

    const contents = await fs.readFile(mainLogFile)
    assert.match(contents, /FileLogger opened/)
  })

  it('configures the worker thread logger', async function () {
    await watcher.configure({workerLog: workerLogFile})

    const contents = await fs.readFile(workerLogFile)
    assert.match(contents, /FileLogger opened/)
  })

  describe('for the polling thread', function () {
    let sub

    afterEach(async function () {
      if (sub) await sub.unwatch()
    })

    describe("while it's stopped", function () {
      it('configures the logger', async function () {
        await watcher.configure({pollingLog: pollingLogFile})

        assert.isFalse(await fs.pathExists(pollingLogFile))

        sub = await watcher.watch(watchDir, {poll: true}, () => {})

        const contents = await fs.readFile(pollingLogFile)
        assert.match(contents, /FileLogger opened/)
      })
    })

    describe("after it's started", function () {
      it('configures the logger', async function () {
        sub = await watcher.watch(watchDir, {poll: true}, () => {})

        await watcher.configure({pollingLog: pollingLogFile})

        const contents = await fs.readFile(pollingLogFile)
        assert.match(contents, /FileLogger opened/)
      })
    })
  })
})
