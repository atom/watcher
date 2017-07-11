const sfw = require('../lib')

const path = require('path')
const fs = require('mz/fs')

describe('entry point', function () {
  let mainLogFile

  beforeEach(function () {
    mainLogFile = path.join(__dirname, '..', 'main.log')
  })

  afterEach(async function () {
    await fs.unlink(mainLogFile).catch(() => {})
  })

  describe('configuration', function () {
    it('validates its arguments', function () {
      assert.throws(() => sfw.configure(), 'requires an option object')
    })

    it('configures the main thread logger', async function () {
      sfw.configure({mainLogFile})

      const contents = await fs.readFile(mainLogFile)
      assert.match(contents, /FileLogger opened/)
    })
  })
})
