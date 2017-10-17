const fs = require('fs-extra')

const {Fixture} = require('../helper')
const {EventMatcher} = require('./matcher');

[false, true].forEach(poll => {
  describe(`basic events with poll = ${poll}`, function () {
    let fixture, matcher

    beforeEach(async function () {
      fixture = new Fixture()
      await fixture.before()
      await fixture.log()

      matcher = new EventMatcher(fixture)
      await matcher.watch({poll})
    })

    afterEach(async function () {
      await fixture.after(this.currentTest)
    })

    it('when a file is created', async function () {
      const createdFile = fixture.watchPath('file.txt')
      await fs.writeFile(createdFile, 'contents')

      await until('the creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: createdFile}
      ))
    })

    it('when a file is modified', async function () {
      const modifiedFile = fixture.watchPath('file.txt')
      await fs.writeFile(modifiedFile, 'initial contents\n')

      await until('the creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: modifiedFile}
      ))

      await fs.appendFile(modifiedFile, 'changed contents\n')
      await until('the modification event arrives', matcher.allEvents(
        {action: 'modified', kind: 'file', path: modifiedFile}
      ))
    })

    it('when a file is renamed', async function () {
      const oldPath = fixture.watchPath('old-file.txt')
      await fs.writeFile(oldPath, 'initial contents\n')

      await until('the creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: oldPath}
      ))

      const newPath = fixture.watchPath('new-file.txt')

      await fs.rename(oldPath, newPath)

      if (poll) {
        await until('the deletion and creation events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'file', path: oldPath},
          {action: 'created', kind: 'file', path: newPath}
        ))
      } else {
        await until('the rename event arrives', matcher.allEvents({
          action: 'renamed', kind: 'file', oldPath, path: newPath
        }))
      }
    })

    it('when a file is deleted', async function () {
      const deletedPath = fixture.watchPath('file.txt')
      await fs.writeFile(deletedPath, 'initial contents\n')

      await until('the creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: deletedPath}
      ))

      await fs.unlink(deletedPath)

      await until('the deletion event arrives', matcher.allEvents(
        {action: 'deleted', path: deletedPath}
      ))
    })

    it('when a directory is created', async function () {
      const subdir = fixture.watchPath('subdir')
      await fs.mkdirs(subdir)

      await until('directory creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'directory', path: subdir}
      ))
    })

    it('when a directory is renamed', async function () {
      const oldDir = fixture.watchPath('subdir')
      const newDir = fixture.watchPath('newdir')

      await fs.mkdirs(oldDir)
      await until('directory creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'directory', path: oldDir}
      ))

      await fs.rename(oldDir, newDir)
      if (poll) {
        await until('directory creation and deletion events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'directory', path: oldDir},
          {action: 'created', kind: 'directory', path: newDir}
        ))
      } else {
        await until('directory rename event arrives', matcher.allEvents(
          {action: 'renamed', kind: 'directory', oldPath: oldDir, path: newDir}
        ))
      }
    })

    it('when a directory is deleted', async function () {
      const subdir = fixture.watchPath('subdir')
      await fs.mkdirs(subdir)
      await until('directory creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'directory', path: subdir}
      ))

      await fs.rmdir(subdir)
      await until('directory deletion event arrives', matcher.allEvents(
        {action: 'deleted', path: subdir}
      ))
    })
  })
})
