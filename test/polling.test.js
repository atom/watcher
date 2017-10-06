const watcher = require('../lib')
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
      assert.equal(watcher.status().pollingThreadState, 'stopped')
    })

    it('runs the polling thread when polling a directory for changes', async function () {
      const sub = await fixture.watch([], {poll: true}, () => {})
      assert.equal(watcher.status().pollingThreadState, 'running')

      await sub.unwatch()
      await until(() => watcher.status().pollingThreadState === 'stopped')
    })
  })
})
