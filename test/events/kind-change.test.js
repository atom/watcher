const fs = require('fs-extra')

const { Fixture } = require('../helper')
const { EventMatcher } = require('../matcher');

[false, true].forEach(poll => {
  describe(`entry kind change events with poll = ${poll}`, function () {
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

    it('when a directory is deleted and a file is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      await fs.mkdir(reusedPath)
      await until('directory creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'directory', path: reusedPath }
      ))

      await fs.rmdir(reusedPath)
      await fs.writeFile(reusedPath, 'IMMA FILE NOW, SURPRIIIISE\n')

      await until('deletion and creation events arrive', matcher.allEvents(
        { action: 'deleted', kind: 'directory', path: reusedPath },
        { action: 'created', kind: 'file', path: reusedPath }
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
        { action: 'created', kind: 'directory', path: reusedPath },
        { action: 'created', kind: 'file', path: oldFilePath }
      ))

      await fs.rmdir(reusedPath)
      await fs.rename(oldFilePath, reusedPath)

      if (poll) {
        await until('deletion and creation events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'directory', path: reusedPath },
          { action: 'deleted', kind: 'file', path: oldFilePath },
          { action: 'created', kind: 'file', path: reusedPath }
        ))
      } else {
        await until('deletion and rename events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'directory', path: reusedPath },
          { action: 'renamed', kind: 'file', oldPath: oldFilePath, path: reusedPath }
        ))
      }
    })

    it('when a directory is renamed and a file is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const newDirPath = fixture.watchPath('newdir')

      await fs.mkdirs(reusedPath)
      await until('directory creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'directory', path: reusedPath }
      ))

      await fs.rename(reusedPath, newDirPath)
      await fs.writeFile(reusedPath, 'oh look a file\n')

      if (poll) {
        await until('rename and creation events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'directory', path: reusedPath },
          { action: 'created', kind: 'directory', path: newDirPath },
          { action: 'created', kind: 'file', path: reusedPath }
        ))
      } else {
        await until('rename and creation events arrive', matcher.allEvents(
          { action: 'renamed', kind: 'directory', oldPath: reusedPath, path: newDirPath },
          { action: 'created', kind: 'file', path: reusedPath }
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
        { action: 'created', kind: 'directory', path: reusedPath },
        { action: 'created', kind: 'file', path: oldFilePath }
      ))

      await fs.rename(reusedPath, newDirPath)
      await fs.rename(oldFilePath, reusedPath)

      if (poll) {
        await until('deletion and creation events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'directory', path: reusedPath },
          { action: 'created', kind: 'directory', path: newDirPath },
          { action: 'deleted', kind: 'file', path: oldFilePath },
          { action: 'created', kind: 'file', path: reusedPath }
        ))
      } else {
        await until('rename events arrive', matcher.allEvents(
          { action: 'renamed', kind: 'directory', oldPath: reusedPath, path: newDirPath },
          { action: 'renamed', kind: 'file', oldPath: oldFilePath, path: reusedPath }
        ))
      }
    })

    it('when a file is deleted and a directory is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      await fs.writeFile(reusedPath, 'something\n')
      await until('directory creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'file', path: reusedPath }
      ))

      await fs.unlink(reusedPath)
      await fs.mkdir(reusedPath)

      await until('delete and create events arrive', matcher.allEvents(
        { action: 'deleted', path: reusedPath },
        { action: 'created', kind: 'directory', path: reusedPath }
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
        { action: 'created', kind: 'file', path: reusedPath },
        { action: 'created', kind: 'directory', path: oldDirPath }
      ))

      await fs.unlink(reusedPath)
      await fs.rename(oldDirPath, reusedPath)

      if (poll) {
        await until('delete and create events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'file', path: reusedPath },
          { action: 'deleted', kind: 'directory', path: oldDirPath },
          { action: 'created', kind: 'directory', path: reusedPath }
        ))
      } else {
        await until('delete and rename events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'file', path: reusedPath },
          { action: 'renamed', kind: 'directory', oldPath: oldDirPath, path: reusedPath }
        ))
      }
    })

    it('when a file is renamed and a directory is created in its place', async function () {
      const reusedPath = fixture.watchPath('reused')
      const newFilePath = fixture.watchPath('newfile')

      await fs.writeFile(reusedPath, 'something\n')
      await until('directory creation event arrives', matcher.allEvents(
        { action: 'created', kind: 'file', path: reusedPath }
      ))

      await fs.rename(reusedPath, newFilePath)
      await fs.mkdir(reusedPath)

      if (poll) {
        await until('creation and deletion events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'file', path: reusedPath },
          { action: 'created', kind: 'file', path: newFilePath },
          { action: 'created', kind: 'directory', path: reusedPath }
        ))
      } else {
        await until('rename and create events arrive', matcher.allEvents(
          { action: 'renamed', kind: 'file', oldPath: reusedPath, path: newFilePath },
          { action: 'created', kind: 'directory', path: reusedPath }
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
        { action: 'created', kind: 'file', path: reusedPath },
        { action: 'created', kind: 'directory', path: oldDirPath }
      ))

      await fs.rename(reusedPath, newFilePath)
      await fs.rename(oldDirPath, reusedPath)

      if (poll) {
        await until('creation and deletion events arrive', matcher.allEvents(
          { action: 'deleted', kind: 'file', path: reusedPath },
          { action: 'created', kind: 'file', path: newFilePath },
          { action: 'deleted', kind: 'directory', path: oldDirPath },
          { action: 'created', kind: 'directory', path: reusedPath }
        ))
      } else {
        await until('rename events arrive', matcher.allEvents(
          { action: 'renamed', kind: 'file', oldPath: reusedPath, path: newFilePath },
          { action: 'renamed', kind: 'directory', oldPath: oldDirPath, path: reusedPath }
        ))
      }
    })
  })
})
