const {status} = require('../lib/native-watcher')
const {Fixture} = require('./helper')

describe('polling', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  describe('thread state', function () {
    it('does not run the polling thread while no paths are being polled', function () {
      assert.equal(status().pollingThreadState, 'stopped')
    })

    it('runs the polling thread when polling a directory for changes', async function () {
      const watcher = await fixture.watch([], {poll: true}, () => {})
      assert.equal(status().pollingThreadState, 'running')

      await watcher.stop()
      await until(() => status().pollingThreadState === 'stopped')
    })
  })
})
