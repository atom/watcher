const fs = require('fs-extra')

const { Fixture } = require('../helper')
const { EventMatcher } = require('../matcher');

[false, true].forEach(poll => {
  describe(`basic events with poll = ${poll}`, function () {
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

    it('when a file is created', async function () {
      const createdFile = fixture.watchPath('file.txt')
      await fs.writeFile(createdFile, 'contents')

      await until('the creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'file', path: createdFile }
      ))
    })

    it('when a file is modified', async function () {
      const modifiedFile = fixture.watchPath('file.txt')
      await fs.writeFile(modifiedFile, 'initial contents\n')

      await until('the creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'file', path: modifiedFile }
      ))

      await fs.appendFile(modifiedFile, 'changed contents\n')
      await until('the modification event arrives', matcher.allEvents(
        { action: 'modified', kind: 'file', path: modifiedFile }
      ))
    })

    it('when a file is renamed', async function () {
      const oldPath = fixture.watchPath('old-file.txt')
      await fs.writeFile(oldPath, 'initial contents\n')

      await until('the creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'file', path: oldPath }
      ))

      const newPath = fixture.watchPath('new-file.txt')

      await fs.rename(oldPath, newPath)

      if (poll) {
        await until('the deletion and creation events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'file', path: oldPath },
          { action: 'created', kind: 'file', path: newPath }
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
        { action: 'created', kind: 'file', path: deletedPath }
      ))

      await fs.unlink(deletedPath)

      await until('the deletion event arrives', matcher.allEvents(
        { action: 'deleted', kind: 'file', path: deletedPath }
      ))
    })

    it('when a directory is created', async function () {
      const subdir = fixture.watchPath('subdir')
      await fs.mkdirs(subdir)

      await until('directory creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'directory', path: subdir }
      ))
    })

    it('when a directory is renamed', async function () {
      const oldDir = fixture.watchPath('subdir')
      const newDir = fixture.watchPath('newdir')

      await fs.mkdirs(oldDir)
      await until('directory creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'directory', path: oldDir }
      ))

      await fs.rename(oldDir, newDir)
      if (poll) {
        await until('directory creation and deletion events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'directory', path: oldDir },
          { action: 'created', kind: 'directory', path: newDir }
        ))
      } else {
        await until('directory rename event arrives', matcher.allEvents(
          { action: 'renamed', kind: 'directory', oldPath: oldDir, path: newDir }
        ))
      }
    })

    it('when a directory is deleted', async function () {
      const subdir = fixture.watchPath('subdir')
      await fs.mkdirs(subdir)
      await until('directory creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'directory', path: subdir }
      ))

      await fs.rmdir(subdir)
      await until('directory deletion event arrives', matcher.allEvents(
        { action: 'deleted', kind: 'directory', path: subdir }
      ))
    })

    it('when a symlink is created', async function () {
      const originalName = fixture.watchPath('original-file.txt')
      const symlinkName = fixture.watchPath('symlink.txt')
      await fs.writeFile(originalName, 'contents\n')

      await until('file creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'file', path: originalName }
      ))

      await fs.symlink(originalName, symlinkName)

      await until('symlink creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'symlink', path: symlinkName }
      ))
    })

    it('when a symlink is deleted', async function () {
      const originalName = fixture.watchPath('original-file.txt')
      const symlinkName = fixture.watchPath('symlink.txt')
      await fs.writeFile(originalName, 'contents\n')
      await fs.symlink(originalName, symlinkName)

      await until('file and symlink creation events arrive', matcher.allEvents(
        { action: 'created', kind: 'file', path: originalName },
        { action: 'created', kind: 'symlink', path: symlinkName }
      ))

      await fs.unlink(symlinkName)

      await until('symlink deletion event arrives', matcher.allEvents(
        { action: 'deleted', kind: 'symlink', path: symlinkName }
      ))
    })

    it('when a symlink is renamed', async function () {
      const targetName = fixture.watchPath('target.txt')
      const originalName = fixture.watchPath('original.txt')
      const finalName = fixture.watchPath('final.txt')
      await fs.writeFile(targetName, 'contents\n')
      await fs.symlink(targetName, originalName)

      await until('file and symlink creation events arrive', matcher.allEvents(
        { action: 'created', kind: 'file', path: targetName },
        { action: 'created', kind: 'symlink', path: originalName }
      ))

      fs.rename(originalName, finalName)

      if (poll) {
        await until('symlink deletion and creation arrive', matcher.allEvents(
          { action: 'deleted', kind: 'symlink', path: originalName },
          { action: 'created', kind: 'symlink', path: finalName }
        ))
      } else {
        await until('rename event arrives', matcher.allEvents(
          { action: 'renamed', kind: 'symlink', oldPath: originalName, path: finalName }
        ))
      }
    })

    if (process.platform === 'win32') {
      it('reports events with the long file name when possible', async function () {
        const longName = fixture.watchPath('file-with-a-long-name.txt')
        const shortName = fixture.watchPath('FILE-W~1.TXT')

        const fd = await fs.open(longName, 'w')
        await fs.close(fd)

        await until('the creation event arrives', matcher.allEvents(
          { action: 'created', kind: 'file', path: longName }
        ))
        assert.isTrue(matcher.noEvents(
          { action: 'modified', kind: 'file', path: longName }
        ))

        await fs.appendFile(shortName, 'contents\n')
        await until('the modification event arrives', matcher.allEvents(
          { action: 'modified', kind: 'file', path: longName }
        ))
        assert.isTrue(matcher.noEvents({ path: shortName }))
      })
    }
  })
})
