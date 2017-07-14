const sfw = require('../lib')

const path = require('path')
const fs = require('mz/fs')

describe('entry point', function () {
  let mainLogFile, workerLogFile

  beforeEach(function () {
    mainLogFile = path.join(__dirname, '..', 'main.test.log')
    workerLogFile = path.join(__dirname, '..', 'worker.test.log')
  })

  afterEach(async function () {
    if (this.currentTest.state === 'failed') {
      const [mainLog, workerLog] = await Promise.all(
        [mainLogFile, workerLogFile].map(fname => fs.readFile(fname, {encoding: 'utf8'}).catch(() => ''))
      )

      console.log(`main log:\n${mainLog}`)
      console.log(`worker log:\n${workerLog}`)
    }

    await Promise.all(
      [mainLogFile, workerLogFile].map(fname => fs.unlink(fname).catch(() => {}))
    )
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
})
