const sfw = require('../lib')

const path = require('path')
const fs = require('fs-extra')

describe('entry point', function () {
  let subs, fixtureDir, watchDir, mainLogFile, workerLogFile

  beforeEach(async function () {
    subs = []

    fixtureDir = path.join(__dirname, 'fixture')
    watchDir = path.join(fixtureDir, 'watched')

    mainLogFile = path.join(fixtureDir, 'main.test.log')
    workerLogFile = path.join(fixtureDir, 'worker.test.log')

    await fs.mkdir(watchDir)
  })

  afterEach(async function () {
    if (this.currentTest.state === 'failed') {
      const [mainLog, workerLog] = await Promise.all(
        [mainLogFile, workerLogFile].map(fname => fs.readFile(fname, {encoding: 'utf8'}).catch(() => ''))
      )

      console.log(`main log:\n${mainLog}`)
      console.log(`worker log:\n${workerLog}`)
    }

    const promises = [mainLogFile, workerLogFile].map(fname => fs.unlink(fname).catch(() => {}))
    promises.push(fs.remove(watchDir))
    promises.push(...subs.map(sub => sub.unwatch()))

    await Promise.all(promises)
  })

  describe('configuration', function () {
    it('validates its arguments', async function () {
      await assert.isRejected(sfw.configure(), /requires an option object/)
    })

    it('configures the main thread logger', async function () {
      await sfw.configure({mainLogFile})

      const contents = await fs.readFile(mainLogFile)
      assert.match(contents, /FileLogger opened/)
    })

    it('configures the worker thread logger', async function () {
      await sfw.configure({workerLogFile})

      const contents = await fs.readFile(workerLogFile)
      assert.match(contents, /FileLogger opened/)
    })
  })

  describe('watching a directory', function () {
    beforeEach(async function () {
      await sfw.configure({mainLogFile, workerLogFile})
    })

    it('begins receiving events within that directory', async function () {
      let error = null
      const events = []

      subs.push(await sfw.watch(watchDir, (err, es) => {
        error = err
        events.push(...es)
      }))

      await fs.writeFile(path.join(watchDir, 'file.txt'), 'indeed')

      await until('an event arrives', () => events.length > 0)
      assert.isNull(error)
    })

    it('can watch multiple directories at once and dispatch events appropriately', async function () {
      let errors = []
      const eventsA = []
      const eventsB = []

      const watchDirA = path.join(watchDir, 'dir_a')
      const watchDirB = path.join(watchDir, 'dir_b')
      await Promise.all(
        [watchDirA, watchDirB].map(subdir => fs.mkdir(subdir))
      )

      subs.push(await sfw.watch(watchDirA, (err, es) => {
        errors.push(err)
        eventsA.push(...es)
      }))
      subs.push(await sfw.watch(watchDirB, (err, es) => {
        errors.push(err)
        eventsB.push(...es)
      }))

      const fileA = path.join(watchDirA, 'a.txt')
      await fs.writeFile(fileA, 'file a')
      await until('watcher A picks up its event', () => eventsA.some(event => event.oldPath === fileA))

      const fileB = path.join(watchDirB, 'b.txt')
      await fs.writeFile(fileB, 'file b')
      await until('watcher B picks up its event', () => eventsB.some(event => event.oldPath === fileB))

      // Assert that the streams weren't crossed
      assert.isTrue(errors.every(err => err === null))
      assert.isTrue(eventsA.every(event => event.oldPath !== fileB))
      assert.isTrue(eventsB.every(event => event.oldPath !== fileA))
    })

    describe('events', function () {
      it('when a file is created')
      it('when a file is modified')
      it('when a file is renamed')
      it('when a file is deleted')
      it('when a directory is created')
      it('when a directory is renamed')
      it('when a directory is deleted')
    })
  })

  describe('unwatching a directory', function () {
    it('unwatches a previously watched directory')
    it('is a no-op if the directory is not being watched')
  })
})
