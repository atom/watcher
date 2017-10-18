const fs = require('fs-extra')

const {Fixture} = require('../helper')
const {EventMatcher} = require('../matcher');

[false, true].forEach(poll => {
  describe(`rapid events with poll = ${poll}`, function () {
    let fixture, matcher

    beforeEach(async function () {
      fixture = new Fixture()
      await fixture.before()
      await fixture.log()

      matcher = new EventMatcher(fixture)
      await matcher.watch([], {poll})
    })

    afterEach(async function () {
      await fixture.after(this.currentTest)
    })

    // The polling thread will never be able to distinguish rapid events
    if (!poll) {
      it('understands coalesced creation and deletion events', async function () {
        const deletedPath = fixture.watchPath('deleted.txt')
        const recreatedPath = fixture.watchPath('recreated.txt')
        const createdPath = fixture.watchPath('created.txt')

        await fs.writeFile(deletedPath, 'initial contents\n')
        await until('file creation event arrives', matcher.allEvents(
          {action: 'created', kind: 'file', path: deletedPath}
        ))

        await fs.unlink(deletedPath)
        await fs.writeFile(recreatedPath, 'initial contents\n')
        await fs.unlink(recreatedPath)
        await fs.writeFile(recreatedPath, 'newly created\n')
        await fs.writeFile(createdPath, 'and another\n')

        await until('all events arrive', matcher.orderedEvents(
          {action: 'deleted', path: deletedPath},
          {action: 'created', kind: 'file', path: recreatedPath},
          {action: 'deleted', path: recreatedPath},
          {action: 'created', kind: 'file', path: recreatedPath},
          {action: 'created', kind: 'file', path: createdPath}
        ))
      })
    }

    it('correlates rapid file rename events', async function () {
      const oldPath0 = fixture.watchPath('old-file-0.txt')
      const oldPath1 = fixture.watchPath('old-file-1.txt')
      const oldPath2 = fixture.watchPath('old-file-2.txt')
      const newPath0 = fixture.watchPath('new-file-0.txt')
      const newPath1 = fixture.watchPath('new-file-1.txt')
      const newPath2 = fixture.watchPath('new-file-2.txt')

      await Promise.all(
        [oldPath0, oldPath1, oldPath2].map(oldPath => fs.writeFile(oldPath, 'original\n'))
      )
      await until('all creation events arrive', matcher.allEvents(
        {action: 'created', kind: 'file', path: oldPath0},
        {action: 'created', kind: 'file', path: oldPath1},
        {action: 'created', kind: 'file', path: oldPath2}
      ))

      await Promise.all([
        fs.rename(oldPath0, newPath0),
        fs.rename(oldPath1, newPath1),
        fs.rename(oldPath2, newPath2)
      ])

      if (poll) {
        await until('all deletion and creation events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'file', path: oldPath0},
          {action: 'deleted', kind: 'file', path: oldPath1},
          {action: 'deleted', kind: 'file', path: oldPath2},
          {action: 'created', kind: 'file', path: newPath0},
          {action: 'created', kind: 'file', path: newPath1},
          {action: 'created', kind: 'file', path: newPath2}
        ))
      } else {
        await until('all rename events arrive', matcher.allEvents(
          {action: 'renamed', kind: 'file', oldPath: oldPath0, path: newPath0},
          {action: 'renamed', kind: 'file', oldPath: oldPath1, path: newPath1},
          {action: 'renamed', kind: 'file', oldPath: oldPath2, path: newPath2}
        ))
      }
    })
  })
})
