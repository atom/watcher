const fs = require('fs-extra')
const path = require('path')

const watcher = require('../lib')

const {prepareFixtureDir, reportLogs, cleanupFixtureDir} = require('./helper');

[false, true].forEach(poll => {
  describe(`events with poll = ${poll}`, function () {
    let fixtureDir, watchDir, mainLogFile, workerLogFile, pollingLogFile
    let errors, events, sub

    beforeEach(async function () {
      ({fixtureDir, watchDir, mainLogFile, workerLogFile, pollingLogFile} = await prepareFixtureDir())
      errors = []
      events = []

      await watcher.configure({mainLog: mainLogFile, workerLog: workerLogFile, pollingLog: pollingLogFile})

      sub = await watcher.watch(watchDir, {poll}, (err, es) => {
        errors.push(err)
        events.push(...es)
      })
    })

    afterEach(async function () {
      await sub.unwatch()
      await reportLogs(this.currentTest, mainLogFile, workerLogFile, pollingLogFile)
      await cleanupFixtureDir(fixtureDir)
    })

    function specMatches (spec, event) {
      return (spec.action === undefined || event.action === spec.action) &&
        (spec.kind === undefined || event.kind === spec.kind) &&
        (spec.path === undefined || event.path === spec.path) &&
        (spec.oldPath === undefined || event.oldPath === spec.oldPath)
    }

    function eventMatching (spec) {
      const isMatch = specMatches.bind(null, spec)
      return function () {
        return events.some(isMatch)
      }
    }

    function allEventsMatching (...specs) {
      const remaining = new Set(specs)

      return function () {
        for (const event of events) {
          for (const spec of remaining) {
            if (specMatches(spec, event)) {
              remaining.delete(spec)
            }
          }
        }

        return remaining.size === 0
      }
    }

    function orderedEventsMatching (...specs) {
      return function () {
        let specIndex = 0

        for (const event of events) {
          if (specs[specIndex] && specMatches(specs[specIndex], event)) {
            specIndex++
          }
        }

        return specIndex >= specs.length
      }
    }

    it('when a file is created', async function () {
      const createdFile = path.join(watchDir, 'file.txt')
      await fs.writeFile(createdFile, 'contents')

      await until('the creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: createdFile}
      ))
    })

    it('when a file is modified', async function () {
      const modifiedFile = path.join(watchDir, 'file.txt')
      await fs.writeFile(modifiedFile, 'initial contents\n')

      await until('the creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: modifiedFile}
      ))

      await fs.appendFile(modifiedFile, 'changed contents\n')
      await until('the modification event arrives', eventMatching(
        {action: 'modified', kind: 'file', path: modifiedFile}
      ))
    })

    it('when a file is renamed', async function () {
      const oldPath = path.join(watchDir, 'old-file.txt')
      await fs.writeFile(oldPath, 'initial contents\n')

      await until('the creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: oldPath}
      ))

      const newPath = path.join(watchDir, 'new-file.txt')

      await fs.rename(oldPath, newPath)

      if (poll) {
        await until('the deletion and creation events arrive', allEventsMatching(
          {action: 'deleted', kind: 'file', path: oldPath},
          {action: 'created', kind: 'file', path: newPath}
        ))
      } else {
        await until('the rename event arrives', eventMatching({
          action: 'renamed', kind: 'file', oldPath, path: newPath
        }))
      }
    })

    it('when a file is renamed from outside of the watch root in', async function () {
      const outsideFile = path.join(fixtureDir, 'file.txt')
      const insideFile = path.join(watchDir, 'file.txt')

      await fs.writeFile(outsideFile, 'contents')
      await fs.rename(outsideFile, insideFile)

      await until('the creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: insideFile}
      ))
    })

    it('when a file is renamed from inside of the watch root out ^windows', async function () {
      const outsideFile = path.join(fixtureDir, 'file.txt')
      const insideFile = path.join(watchDir, 'file.txt')
      const flagFile = path.join(watchDir, 'flag.txt')

      await fs.writeFile(insideFile, 'contents')

      await until('the creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: insideFile}
      ))

      await fs.rename(insideFile, outsideFile)
      await fs.writeFile(flagFile, 'flag 1')

      await until('the flag file event arrives', eventMatching(
        {action: 'created', kind: 'file', path: flagFile}
      ))

      // Trigger another batch of events on Linux
      await fs.writeFile(flagFile, 'flag 2')

      await until('the deletion event arrives', eventMatching(
        {action: 'deleted', kind: 'file', path: insideFile}
      ))
    })

    it('when a file is deleted', async function () {
      const deletedPath = path.join(watchDir, 'file.txt')
      await fs.writeFile(deletedPath, 'initial contents\n')

      await until('the creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: deletedPath}
      ))

      await fs.unlink(deletedPath)

      await until('the deletion event arrives', eventMatching(
        {action: 'deleted', path: deletedPath}
      ))
    })

    // The polling thread will never be able to distinguish rapid events
    if (!poll) {
      it('understands coalesced creation and deletion events', async function () {
        const deletedPath = path.join(watchDir, 'deleted.txt')
        const recreatedPath = path.join(watchDir, 'recreated.txt')
        const createdPath = path.join(watchDir, 'created.txt')

        await fs.writeFile(deletedPath, 'initial contents\n')
        await until('file creation event arrives', eventMatching(
          {action: 'created', kind: 'file', path: deletedPath}
        ))

        await fs.unlink(deletedPath)
        await fs.writeFile(recreatedPath, 'initial contents\n')
        await fs.unlink(recreatedPath)
        await fs.writeFile(recreatedPath, 'newly created\n')
        await fs.writeFile(createdPath, 'and another\n')

        await until('all events arrive', orderedEventsMatching(
          {action: 'deleted', path: deletedPath},
          {action: 'created', kind: 'file', path: recreatedPath},
          {action: 'deleted', path: recreatedPath},
          {action: 'created', kind: 'file', path: recreatedPath},
          {action: 'created', kind: 'file', path: createdPath}
        ))
      })
    }

    it('correlates rapid file rename events', async function () {
      const oldPath0 = path.join(watchDir, 'old-file-0.txt')
      const oldPath1 = path.join(watchDir, 'old-file-1.txt')
      const oldPath2 = path.join(watchDir, 'old-file-2.txt')
      const newPath0 = path.join(watchDir, 'new-file-0.txt')
      const newPath1 = path.join(watchDir, 'new-file-1.txt')
      const newPath2 = path.join(watchDir, 'new-file-2.txt')

      await Promise.all(
        [oldPath0, oldPath1, oldPath2].map(oldPath => fs.writeFile(oldPath, 'original\n'))
      )
      await until('all creation events arrive', allEventsMatching(
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
        await until('all deletion and creation events arrive', allEventsMatching(
          {action: 'deleted', kind: 'file', path: oldPath0},
          {action: 'deleted', kind: 'file', path: oldPath1},
          {action: 'deleted', kind: 'file', path: oldPath2},
          {action: 'created', kind: 'file', path: newPath0},
          {action: 'created', kind: 'file', path: newPath1},
          {action: 'created', kind: 'file', path: newPath2}
        ))
      } else {
        await until('all rename events arrive', allEventsMatching(
          {action: 'renamed', kind: 'file', oldPath: oldPath0, path: newPath0},
          {action: 'renamed', kind: 'file', oldPath: oldPath1, path: newPath1},
          {action: 'renamed', kind: 'file', oldPath: oldPath2, path: newPath2}
        ))
      }
    })

    it('when a directory is created', async function () {
      const subdir = path.join(watchDir, 'subdir')
      await fs.mkdirs(subdir)

      await until('directory creation event arrives', eventMatching(
        {action: 'created', kind: 'directory', path: subdir}
      ))
    })

    it('when a directory is renamed', async function () {
      const oldDir = path.join(watchDir, 'subdir')
      const newDir = path.join(watchDir, 'newdir')

      await fs.mkdirs(oldDir)
      await until('directory creation event arrives', eventMatching(
        {action: 'created', kind: 'directory', path: oldDir}
      ))

      await fs.rename(oldDir, newDir)
      if (poll) {
        await until('directory creation and deletion events arrive', eventMatching(
          {action: 'deleted', kind: 'directory', path: oldDir},
          {action: 'created', kind: 'directory', path: newDir}
        ))
      } else {
        await until('directory rename event arrives', eventMatching(
          {action: 'renamed', kind: 'directory', oldPath: oldDir, path: newDir}
        ))
      }
    })

    it('when a directory is deleted', async function () {
      const subdir = path.join(watchDir, 'subdir')
      await fs.mkdirs(subdir)
      await until('directory creation event arrives', eventMatching(
        {action: 'created', kind: 'directory', path: subdir}
      ))

      await fs.rmdir(subdir)
      await until('directory deletion event arrives', eventMatching(
        {action: 'deleted', path: subdir}
      ))
    })

    it('when a directory is deleted and a file is created in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      await fs.mkdir(reusedPath)
      await until('directory creation event arrives', eventMatching(
        {action: 'created', kind: 'directory', path: reusedPath}
      ))

      await fs.rmdir(reusedPath)
      await fs.writeFile(reusedPath, 'IMMA FILE NOW, SURPRIIIISE\n')

      await until('deletion and creation events arrive', orderedEventsMatching(
        {action: 'deleted', path: reusedPath},
        {action: 'created', kind: 'file', path: reusedPath}
      ))
    })

    it('when a directory is deleted and a file is renamed in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      const oldFilePath = path.join(watchDir, 'oldfile')

      await Promise.all([
        fs.mkdirs(reusedPath),
        fs.writeFile(oldFilePath, 'original\n')
      ])
      await until('directory and file creation events arrive', allEventsMatching(
        {action: 'created', kind: 'directory', path: reusedPath},
        {action: 'created', kind: 'file', path: oldFilePath}
      ))

      await fs.rmdir(reusedPath)
      await fs.rename(oldFilePath, reusedPath)

      if (poll) {
        await until('deletion and creation events arrive', allEventsMatching(
          {action: 'deleted', kind: 'directory', path: reusedPath},
          {action: 'deleted', kind: 'file', path: oldFilePath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      } else {
        await until('deletion and rename events arrive', allEventsMatching(
          {action: 'deleted', path: reusedPath},
          {action: 'renamed', kind: 'file', oldPath: oldFilePath, path: reusedPath}
        ))
      }
    })

    it('when a directory is renamed and a file is created in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      const newDirPath = path.join(watchDir, 'newdir')

      await fs.mkdirs(reusedPath)
      await until('directory creation event arrives', eventMatching(
        {action: 'created', kind: 'directory', path: reusedPath}
      ))

      await fs.rename(reusedPath, newDirPath)
      await fs.writeFile(reusedPath, 'oh look a file\n')

      if (poll) {
        await until('rename and creation events arrive', allEventsMatching(
          {action: 'deleted', kind: 'directory', path: reusedPath},
          {action: 'created', kind: 'directory', path: newDirPath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      } else {
        await until('rename and creation events arrive', allEventsMatching(
          {action: 'renamed', kind: 'directory', oldPath: reusedPath, path: newDirPath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      }
    })

    it('when a directory is renamed and a file is renamed in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      const oldFilePath = path.join(watchDir, 'oldfile')
      const newDirPath = path.join(watchDir, 'newdir')

      await Promise.all([
        fs.mkdirs(reusedPath),
        fs.writeFile(oldFilePath, 'started as a file\n')
      ])
      await until('directory and file creation events arrive', allEventsMatching(
        {action: 'created', kind: 'directory', path: reusedPath},
        {action: 'created', kind: 'file', path: oldFilePath}
      ))

      await fs.rename(reusedPath, newDirPath)
      await fs.rename(oldFilePath, reusedPath)

      if (poll) {
        await until('deletion and creation events arrive', allEventsMatching(
          {action: 'deleted', kind: 'directory', path: reusedPath},
          {action: 'created', kind: 'directory', path: newDirPath},
          {action: 'deleted', kind: 'file', path: oldFilePath},
          {action: 'created', kind: 'file', path: reusedPath}
        ))
      } else {
        await until('rename events arrive', allEventsMatching(
          {action: 'renamed', kind: 'directory', oldPath: reusedPath, path: newDirPath},
          {action: 'renamed', kind: 'file', oldPath: oldFilePath, path: reusedPath}
        ))
      }
    })

    it('when a file is deleted and a directory is created in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      await fs.writeFile(reusedPath, 'something\n')
      await until('directory creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: reusedPath}
      ))

      await fs.unlink(reusedPath)
      await fs.mkdir(reusedPath)

      await until('delete and create events arrive', orderedEventsMatching(
        {action: 'deleted', path: reusedPath},
        {action: 'created', kind: 'directory', path: reusedPath}
      ))
    })

    it('when a file is deleted and a directory is renamed in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      const oldDirPath = path.join(watchDir, 'olddir')

      await Promise.all([
        fs.writeFile(reusedPath, 'something\n'),
        fs.mkdir(oldDirPath)
      ])
      await until('creation events arrive', allEventsMatching(
        {action: 'created', kind: 'file', path: reusedPath},
        {action: 'created', kind: 'directory', path: oldDirPath}
      ))

      await fs.unlink(reusedPath)
      await fs.rename(oldDirPath, reusedPath)

      if (poll) {
        await until('delete and create events arrive', allEventsMatching(
          {action: 'deleted', path: reusedPath},
          {action: 'deleted', kind: 'directory', path: oldDirPath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      } else {
        await until('delete and rename events arrive', allEventsMatching(
          {action: 'deleted', path: reusedPath},
          {action: 'renamed', kind: 'directory', oldPath: oldDirPath, path: reusedPath}
        ))
      }
    })

    it('when a file is renamed and a directory is created in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      const newFilePath = path.join(watchDir, 'newfile')

      await fs.writeFile(reusedPath, 'something\n')
      await until('directory creation event arrives', eventMatching(
        {action: 'created', kind: 'file', path: reusedPath}
      ))

      await fs.rename(reusedPath, newFilePath)
      await fs.mkdir(reusedPath)

      if (poll) {
        await until('creation and deletion events arrive', allEventsMatching(
          {action: 'deleted', kind: 'file', path: reusedPath},
          {action: 'created', kind: 'file', path: newFilePath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      } else {
        await until('rename and create events arrive', orderedEventsMatching(
          {action: 'renamed', kind: 'file', oldPath: reusedPath, path: newFilePath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      }
    })

    it('when a file is renamed and a directory is renamed in its place', async function () {
      const reusedPath = path.join(watchDir, 'reused')
      const oldDirPath = path.join(watchDir, 'olddir')
      const newFilePath = path.join(watchDir, 'newfile')

      await Promise.all([
        fs.writeFile(reusedPath, 'something\n'),
        fs.mkdir(oldDirPath)
      ])
      await until('file and directory creation events arrive', allEventsMatching(
        {action: 'created', kind: 'file', path: reusedPath},
        {action: 'created', kind: 'directory', path: oldDirPath}
      ))

      await fs.rename(reusedPath, newFilePath)
      await fs.rename(oldDirPath, reusedPath)

      if (poll) {
        await until('creation and deletion events arrive', allEventsMatching(
          {action: 'deleted', kind: 'file', path: reusedPath},
          {action: 'created', kind: 'file', path: newFilePath},
          {action: 'deleted', kind: 'directory', path: oldDirPath},
          {action: 'created', kind: 'directory', path: reusedPath}
        ))
      } else {
        await until('rename events arrive', orderedEventsMatching(
          {action: 'renamed', kind: 'file', oldPath: reusedPath, path: newFilePath},
          {action: 'renamed', kind: 'directory', oldPath: oldDirPath, path: reusedPath}
        ))
      }
    })
  })
})
