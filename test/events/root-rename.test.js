const fs = require('fs-extra')

const { Fixture } = require('../helper')
const { EventMatcher } = require('../matcher');

[false, true].forEach(poll => {
  describe(`renaming the root with poll = ${poll}`, function () {
    let fixture, matcher

    beforeEach(async function () {
      fixture = new Fixture()
      await fixture.before()
      await fixture.log()

      matcher = new EventMatcher(fixture)
      await matcher.watch([], { poll })
    })

    afterEach(async function () {
      await fixture.after(this.currentTest)
    })

    it('emits a deletion event for the root move itself ^windows', async function () {
      const oldRoot = fixture.watchPath()
      const newRoot = fixture.fixturePath('new-root')

      await fs.rename(oldRoot, newRoot)

      await until('deletion event arrives', matcher.allEvents(
        { action: 'deleted', kind: 'directory', path: oldRoot }
      ))
    })

    it('does not emit events within the new root ^windows', async function () {
      const oldRoot = fixture.watchPath()
      const oldFile = fixture.watchPath('some-file.txt')
      const newRoot = fixture.fixturePath('new-root')
      const newFile = fixture.fixturePath('new-root', 'some-file.txt')

      await fs.rename(oldRoot, newRoot)
      await fs.appendFile(newFile, 'changed\n')

      await until('deletion event arrives', matcher.allEvents(
        { action: 'deleted', kind: 'directory', path: oldRoot }
      ))

      assert.isTrue(matcher.noEvents(
        { path: oldFile },
        { path: newFile }
      ))
    })
  })
})
