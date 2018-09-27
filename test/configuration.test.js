/* eslint-dev mocha */
const fs = require('fs-extra')

const { configure } = require('../lib/binding')
const { Fixture } = require('./helper')

describe('configuration', function () {
  let fixture, badPath

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()

    badPath = fixture.fixturePath('bad', 'path.log')
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  it('validates its arguments', async function () {
    await assert.isRejected(configure(), /requires an option object/)
  })

  it('configures the main thread logger', async function () {
    await configure({ mainLog: fixture.mainLogFile })

    const contents = await fs.readFile(fixture.mainLogFile)
    assert.match(contents, /FileLogger opened/)
  })

  it('configures the worker thread logger', async function () {
    await configure({ workerLog: fixture.workerLogFile })

    const contents = await fs.readFile(fixture.workerLogFile)
    assert.match(contents, /FileLogger opened/)
  })

  it('fails if the main log file cannot be written', async function () {
    await assert.isRejected(configure({ mainLog: badPath }), /No such file or directory/)
  })

  it('fails if the worker log file cannot be written', async function () {
    await assert.isRejected(configure({ workerLog: badPath }), /No such file or directory/)
  })

  describe('for the polling thread', function () {
    describe("while it's stopped", function () {
      it('configures the logger', async function () {
        await configure({ pollingLog: fixture.pollingLogFile })

        assert.isFalse(await fs.pathExists(fixture.pollingLogFile))

        await fixture.watch([], { poll: true }, () => {})

        const contents = await fs.readFile(fixture.pollingLogFile)
        assert.match(contents, /FileLogger opened/)
      })

      it('defers the check for a valid polling log file', async function () {
        await configure({ pollingLog: badPath })
      })
    })

    describe("after it's started", function () {
      it('configures the logger', async function () {
        await fixture.watch([], { poll: true }, () => {})

        await configure({ pollingLog: fixture.pollingLogFile })

        const contents = await fs.readFile(fixture.pollingLogFile)
        assert.match(contents, /FileLogger opened/)
      })

      it('fails if the polling log file cannot be written', async function () {
        await fixture.watch([], { poll: true }, () => {})

        await assert.isRejected(configure({ pollingLog: badPath }), /No such file or directory/)
      })
    })
  })
})
