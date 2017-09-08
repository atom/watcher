const watcher = require('../lib')

const {prepareFixtureDir, reportLogs, cleanupFixtureDir} = require('./helper')

const path = require('path')
const fs = require('fs-extra')

describe('watcher', function () {
  let subs, fixtureDir, watchDir, mainLogFile, workerLogFile

  beforeEach(async function () {
    subs = [];
    ({fixtureDir, watchDir, mainLogFile, workerLogFile} = await prepareFixtureDir())
  })

  afterEach(async function () {
    await Promise.all(subs.map(sub => sub.unwatch()))
    await reportLogs(this.currentTest, mainLogFile, workerLogFile)
    await cleanupFixtureDir(fixtureDir)
  })

  describe('unwatching a directory', function () {
    beforeEach(async function () {
      await watcher.configure({mainLog: mainLogFile, workerLog: workerLogFile})
    })

    it('unwatches a previously watched directory', async function () {
      let error = null
      const events = []

      const sub = await watcher.watch(watchDir, (err, es) => {
        error = err
        events.push(...es)
      })
      subs.push(sub)

      const filePath = path.join(watchDir, 'file.txt')
      await fs.writeFile(filePath, 'original')

      await until('the event arrives', () => events.some(event => event.oldPath === filePath))
      const eventCount = events.length
      assert.isNull(error)

      await sub.unwatch()

      await fs.writeFile(filePath, 'the modification')

      // Give the modification event a chance to arrive.
      // Not perfect, but adequate.
      await new Promise(resolve => setTimeout(resolve, 100))

      assert.lengthOf(events, eventCount)
    })

    it('is a no-op if the directory is not being watched', async function () {
      let error = null
      const sub = await watcher.watch(watchDir, err => (error = err))
      subs.push(sub)
      assert.isNull(error)

      await sub.unwatch()
      assert.isNull(error)

      await sub.unwatch()
      assert.isNull(error)
    })
  })
})
