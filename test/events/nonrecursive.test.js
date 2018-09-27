const fs = require('fs-extra')

const { Fixture } = require('../helper')
const { EventMatcher } = require('../matcher');

[false, true].forEach(poll => {
  describe(`nonrecursive watching with poll = ${poll}`, function () {
    let fixture, matcher

    beforeEach(async function () {
      fixture = new Fixture()
      await fixture.before()
      await fixture.log()

      await Promise.all([
        fs.mkdir(fixture.watchPath('subdir-0')),
        fs.mkdir(fixture.watchPath('subdir-1'))
      ])

      matcher = new EventMatcher(fixture)
      await matcher.watch([], { poll, recursive: false })
    })

    afterEach(async function () {
      await fixture.after(this.currentTest)
    })

    it('receives events for entries directly within its root', async function () {
      const filePath = fixture.watchPath('file0.txt')
      const dirPath = fixture.watchPath('subdir-2')

      await Promise.all([
        fs.writeFile(filePath, 'yes\n'),
        fs.mkdir(dirPath)
      ])

      await until('creation events arrive', matcher.allEvents(
        { action: 'created', kind: 'file', path: filePath },
        { action: 'created', kind: 'directory', path: dirPath }
      ))

      await Promise.all([
        fs.unlink(filePath),
        fs.rmdir(dirPath)
      ])

      await until('deletion events arrive', matcher.allEvents(
        { action: 'deleted', path: filePath },
        { action: 'deleted', path: dirPath }
      ))
    })

    it('ignores events for entries within a subdirectory', async function () {
      const flagFile = fixture.watchPath('file0.txt')
      const file1Path = fixture.watchPath('subdir-0', 'file1.txt')
      const file2Path = fixture.watchPath('subdir-0', 'file2.txt')
      const file3Path = fixture.watchPath('subdir-1', 'file3.txt')

      await fs.writeFile(file1Path, 'nope\n')
      await fs.writeFile(file2Path, 'nope\n')
      await fs.writeFile(file3Path, 'nope\n')
      await fs.writeFile(flagFile, 'uh huh\n')

      await until('creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'file', path: flagFile }
      ))
      assert.isTrue(matcher.noEvents(
        { path: file1Path },
        { path: file2Path },
        { path: file3Path }
      ))
    })
  })
})
