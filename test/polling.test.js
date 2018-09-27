const { status } = require('../lib/binding')
const { Fixture } = require('./helper')

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
    it('does not run the polling thread while no paths are being polled', async function () {
      const s = await status()
      assert.equal(s.pollingThreadState, 'stopped')
    })

    it('runs the polling thread when polling a directory for changes', async function () {
      const watcher = await fixture.watch([], { poll: true }, () => {})
      const s = await status()
      assert.equal(s.pollingThreadState, 'running')

      await watcher.getNativeWatcher().stop(false)
      await until(async () => (await status()).pollingThreadState === 'stopped')
    })
  })
})
