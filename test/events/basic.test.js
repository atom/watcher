const fs = require('fs-extra')

const {Fixture} = require('../helper')
const {EventMatcher} = require('./matcher');

[false, true].forEach(poll => {
  describe(`events with poll = ${poll}`, function () {
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

    it('when a file is renamed from outside of the watch root in', async function () {
      const outsideFile = fixture.fixturePath('file.txt')
      const insideFile = fixture.watchPath('file.txt')

      await fs.writeFile(outsideFile, 'contents')
      await fs.rename(outsideFile, insideFile)

      await until('the creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: insideFile}
      ))
    })

    it('when a file is renamed from inside of the watch root out', async function () {
      const outsideFile = fixture.fixturePath('file.txt')
      const insideFile = fixture.watchPath('file.txt')
      const flagFile = fixture.watchPath('flag.txt')

      await fs.writeFile(insideFile, 'contents')

      await until('the creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: insideFile}
      ))

      await fs.rename(insideFile, outsideFile)
      await fs.writeFile(flagFile, 'flag 1')

      await until('the flag file event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: flagFile}
      ))

      // Trigger another batch of events on Linux
      await fs.writeFile(flagFile, 'flag 2')

      await until('the deletion event arrives', matcher.allEvents(
        {action: 'deleted', path: insideFile}
      ))
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

    it('when a directory is deleted and a file is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      await fs.mkdir(reusedPath)
      await until('directory creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'directory', path: reusedPath}
      ))

      await fs.rmdir(reusedPath)
      await fs.writeFile(reusedPath, 'IMMA FILE NOW, SURPRIIIISE\n')

      await until('deletion and creation events arrive', matcher.allEvents(
        {action: 'deleted', path: reusedPath},
        {action: 'created', kind: 'file', path: reusedPath}
      ))
    })

    it('when a directory is deleted and a file is renamed in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const oldFilePath = fixture.watchPath('oldfile')

      await Promise.all([
        fs.mkdirs(reusedPath),
        fs.writeFile(oldFilePath, 'original\n')
      ])
      await until('directory and file creation events arrive', matcher.allEvents(
        {action: 'created', kind: 'directory', path: reusedPath},
        {action: 'created', kind: 'file', path: oldFilePath}
      ))

      await fs.rmdir(reusedPath)
      await fs.rename(oldFilePath, reusedPath)

      if (poll) {
        await until('deletion and creation events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'directory', path: reusedPath},
          {action: 'deleted', kind: 'file', path: oldFilePath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      } else {
        await until('deletion and rename events arrive', matcher.allEvents(
          {action: 'deleted', path: reusedPath},
          {action: 'renamed', kind: 'file', oldPath: oldFilePath, path: reusedPath}
        ))
      }
    })

    it('when a directory is renamed and a file is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const newDirPath = fixture.watchPath('newdir')

      await fs.mkdirs(reusedPath)
      await until('directory creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'directory', path: reusedPath}
      ))

      await fs.rename(reusedPath, newDirPath)
      await fs.writeFile(reusedPath, 'oh look a file\n')

      if (poll) {
        await until('rename and creation events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'directory', path: reusedPath},
          {action: 'created', kind: 'directory', path: newDirPath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      } else {
        await until('rename and creation events arrive', matcher.allEvents(
          {action: 'renamed', kind: 'directory', oldPath: reusedPath, path: newDirPath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      }
    })

    it('when a directory is renamed and a file is renamed in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const oldFilePath = fixture.watchPath('oldfile')
      const newDirPath = fixture.watchPath('newdir')

      await Promise.all([
        fs.mkdirs(reusedPath),
        fs.writeFile(oldFilePath, 'started as a file\n')
      ])
      await until('directory and file creation events arrive', matcher.allEvents(
        {action: 'created', kind: 'directory', path: reusedPath},
        {action: 'created', kind: 'file', path: oldFilePath}
      ))

      await fs.rename(reusedPath, newDirPath)
      await fs.rename(oldFilePath, reusedPath)

      if (poll) {
        await until('deletion and creation events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'directory', path: reusedPath},
          {action: 'created', kind: 'directory', path: newDirPath},
          {action: 'deleted', kind: 'file', path: oldFilePath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      } else {
        await until('rename events arrive', matcher.allEvents(
          {action: 'renamed', kind: 'directory', oldPath: reusedPath, path: newDirPath},
          {action: 'renamed', kind: 'file', oldPath: oldFilePath, path: reusedPath}
        ))
      }
    })

    it('when a file is deleted and a directory is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      await fs.writeFile(reusedPath, 'something\n')
      await until('directory creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: reusedPath}
      ))

      await fs.unlink(reusedPath)
      await fs.mkdir(reusedPath)

      await until('delete and create events arrive', matcher.allEvents(
        {action: 'deleted', path: reusedPath},
        {action: 'created', kind: 'directory', path: reusedPath}
      ))
    })

    it('when a file is deleted and a directory is renamed in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const oldDirPath = fixture.watchPath('olddir')

      await Promise.all([
        fs.writeFile(reusedPath, 'something\n'),
        fs.mkdir(oldDirPath)
      ])
      await until('creation events arrive', matcher.allEvents(
        {action: 'created', kind: 'file', path: reusedPath},
        {action: 'created', kind: 'directory', path: oldDirPath}
      ))

      await fs.unlink(reusedPath)
      await fs.rename(oldDirPath, reusedPath)

      if (poll) {
        await until('delete and create events arrive', matcher.allEvents(
          {action: 'deleted', path: reusedPath},
          {action: 'deleted', kind: 'directory', path: oldDirPath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      } else {
        await until('delete and rename events arrive', matcher.allEvents(
          {action: 'deleted', path: reusedPath},
          {action: 'renamed', kind: 'directory', oldPath: oldDirPath, path: reusedPath}
        ))
      }
    })

    it('when a file is renamed and a directory is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const newFilePath = fixture.watchPath('newfile')

      await fs.writeFile(reusedPath, 'something\n')
      await until('directory creation event arrives', matcher.allEvents(
        {action: 'created', kind: 'file', path: reusedPath}
      ))

      await fs.rename(reusedPath, newFilePath)
      await fs.mkdir(reusedPath)

      if (poll) {
        await until('creation and deletion events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'file', path: reusedPath},
          {action: 'created', kind: 'file', path: newFilePath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      } else {
        await until('rename and create events arrive', matcher.allEvents(
          {action: 'renamed', kind: 'file', oldPath: reusedPath, path: newFilePath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      }
    })

    it('when a file is renamed and a directory is renamed in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const oldDirPath = fixture.watchPath('olddir')
      const newFilePath = fixture.watchPath('newfile')

      await Promise.all([
        fs.writeFile(reusedPath, 'something\n'),
        fs.mkdir(oldDirPath)
      ])
      await until('file and directory creation events arrive', matcher.allEvents(
        {action: 'created', kind: 'file', path: reusedPath},
        {action: 'created', kind: 'directory', path: oldDirPath}
      ))

      await fs.rename(reusedPath, newFilePath)
      await fs.rename(oldDirPath, reusedPath)

      if (poll) {
        await until('creation and deletion events arrive', matcher.allEvents(
          {action: 'deleted', kind: 'file', path: reusedPath},
          {action: 'created', kind: 'file', path: newFilePath},
          {action: 'deleted', kind: 'directory', path: oldDirPath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      } else {
        await until('rename events arrive', matcher.allEvents(
          {action: 'renamed', kind: 'file', oldPath: reusedPath, path: newFilePath},
          {action: 'renamed', kind: 'directory', oldPath: oldDirPath, path: reusedPath}
        ))
      }
    })
  })
})
