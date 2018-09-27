const fs = require('fs-extra')
const { Fixture } = require('./helper')

describe('unwatching a directory', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  it('unwatches a previously watched directory', async function () {
    let error = null
    const events = []

    const watcher = await fixture.watch([], {}, (err, es) => {
      error = err
      events.push(...es)
    })

    const filePath = fixture.watchPath('file.txt')
    await fs.writeFile(filePath, 'original')

    await until('the event arrives', () => events.some(event => event.path === filePath))
    assert.isNull(error)

    await watcher.getNativeWatcher().stop(false)
    const eventCount = events.length

    await fs.writeFile(filePath, 'the modification')

    // Give the modification event a chance to arrive.
    // Not perfect, but adequate.
    await new Promise(resolve => setTimeout(resolve, 100))

    assert.lengthOf(events, eventCount)
  })

  it('is a no-op if the directory is not being watched', async function () {
    let error = null
    const watcher = await fixture.watch([], {}, err => (error = err))
    assert.isNull(error)

    const native = watcher.getNativeWatcher()
    await native.stop(false)
    assert.isNull(error)
    assert.isNull(watcher.getNativeWatcher())

    await native.stop(false)
    assert.isNull(error)
  })
})
