const watcher = require('../lib')

const path = require('path')
const fs = require('fs-extra')

describe('entry point', function () {
  let subs, fixtureDir, watchDir, mainLogFile, workerLogFile

  beforeEach(async function () {
    subs = []

    const rootDir = path.join(__dirname, 'fixture')
    fixtureDir = await fs.mkdtemp(path.join(rootDir, 'watched-'))
    watchDir = path.join(fixtureDir, 'root')
    await fs.mkdirs(watchDir)

    mainLogFile = path.join(fixtureDir, 'main.test.log')
    workerLogFile = path.join(fixtureDir, 'worker.test.log')

    await Promise.all([
      [mainLogFile, workerLogFile].map(fname => fs.unlink(fname, {encoding: 'utf8'}).catch(() => ''))
    ])
  })

  afterEach(async function () {
    if (this.currentTest.state === 'failed') {
      const [mainLog, workerLog] = await Promise.all(
        [mainLogFile, workerLogFile].map(fname => fs.readFile(fname, {encoding: 'utf8'}).catch(() => ''))
      )

      console.log(`main log ${mainLogFile}:\n${mainLog}`)
      console.log(`worker log ${workerLogFile}:\n${workerLog}`)
    }

    await Promise.all([
      fs.remove(watchDir),
      ...subs.map(sub => sub.unwatch())
    ])
  })

  describe('configuration', function () {
    it('validates its arguments', async function () {
      await assert.isRejected(watcher.configure(), /requires an option object/)
    })

    it('configures the main thread logger', async function () {
      await watcher.configure({mainLogFile})

      const contents = await fs.readFile(mainLogFile)
      assert.match(contents, /FileLogger opened/)
    })

    it('configures the worker thread logger ^linux', async function () {
      await watcher.configure({workerLogFile})

      const contents = await fs.readFile(workerLogFile)
      assert.match(contents, /FileLogger opened/)
    })
  })

  describe('watching a directory', function () {
    beforeEach(async function () {
      if (!['darwin', 'win32'].includes(process.platform)) {
        this.skip()
      }

      await watcher.configure({mainLogFile, workerLogFile})
    })

    it('begins receiving events within that directory ^linux', async function () {
      let error = null
      const events = []

      subs.push(await watcher.watch(watchDir, (err, es) => {
        error = err
        events.push(...es)
      }))

      await fs.writeFile(path.join(watchDir, 'file.txt'), 'indeed')

      await until('an event arrives', () => events.length > 0)
      assert.isNull(error)
    })

    it('can watch multiple directories at once and dispatch events appropriately ^linux', async function () {
      const errors = []
      const eventsA = []
      const eventsB = []

      const watchDirA = path.join(watchDir, 'dir_a')
      const watchDirB = path.join(watchDir, 'dir_b')
      await Promise.all(
        [watchDirA, watchDirB].map(subdir => fs.mkdir(subdir))
      )

      subs.push(await watcher.watch(watchDirA, (err, es) => {
        errors.push(err)
        eventsA.push(...es)
      }))
      subs.push(await watcher.watch(watchDirB, (err, es) => {
        errors.push(err)
        eventsB.push(...es)
      }))

      const fileA = path.join(watchDirA, 'a.txt')
      await fs.writeFile(fileA, 'file a')
      await until('watcher A picks up its event', () => eventsA.some(event => event.oldPath === fileA))

      const fileB = path.join(watchDirB, 'b.txt')
      await fs.writeFile(fileB, 'file b')
      await until('watcher B picks up its event', () => eventsB.some(event => event.oldPath === fileB))

      // Assert that the streams weren't crossed
      assert.isTrue(errors.every(err => err === null))
      assert.isTrue(eventsA.every(event => event.oldPath !== fileB))
      assert.isTrue(eventsB.every(event => event.oldPath !== fileA))
    })

    describe('events', function () {
      let errors, events

      beforeEach(async function () {
        if (!['darwin'].includes(process.platform)) {
          this.skip()
        }

        errors = []
        events = []

        subs.push(await watcher.watch(watchDir, (err, es) => {
          errors.push(err)
          events.push(...es)
        }))
      })

      function specMatches (spec, event) {
        return (spec.type === undefined || event.type === spec.type) &&
          (event.kind === undefined || event.kind === spec.kind) &&
          (event.oldPath === undefined || event.oldPath === spec.oldPath) &&
          (event.newPath === (spec.newPath || ''))
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
            if (specMatches(specs[specIndex], event)) {
              specIndex++
            }
          }

          return specIndex >= specs.length
        }
      }

      it('when a file is created ^windows ^linux', async function () {
        const createdFile = path.join(watchDir, 'file.txt')
        await fs.writeFile(createdFile, 'contents')

        await until('the creation event arrives', eventMatching({
          type: 'created',
          kind: 'file',
          oldPath: createdFile
        }))
      })

      it('when a file is modified ^windows ^linux', async function () {
        const modifiedFile = path.join(watchDir, 'file.txt')
        await fs.writeFile(modifiedFile, 'initial contents\n')

        await until('the creation event arrives', eventMatching({
          type: 'created',
          kind: 'file',
          oldPath: modifiedFile
        }))

        await fs.appendFile(modifiedFile, 'changed contents\n')
        await until('the modification event arrives', eventMatching({
          type: 'modified',
          kind: 'file',
          oldPath: modifiedFile
        }))
      })

      it('when a file is renamed ^windows ^linux', async function () {
        const oldPath = path.join(watchDir, 'old-file.txt')
        await fs.writeFile(oldPath, 'initial contents\n')

        await until('the creation event arrives', eventMatching({
          type: 'created',
          kind: 'file',
          oldPath,
          newPath: ''
        }))

        const newPath = path.join(watchDir, 'new-file.txt')

        await fs.rename(oldPath, newPath)

        await until('the rename event arrives', eventMatching({
          type: 'renamed',
          kind: 'file',
          oldPath,
          newPath
        }))
      })

      it('when a file is deleted ^windows ^linux', async function () {
        const deletedPath = path.join(watchDir, 'file.txt')
        await fs.writeFile(deletedPath, 'initial contents\n')

        await until('the creation event arrives', eventMatching({
          type: 'created',
          kind: 'file',
          oldPath: deletedPath
        }))

        await fs.unlink(deletedPath)

        await until('the deletion event arrives', eventMatching({
          type: 'deleted',
          kind: 'file',
          oldPath: deletedPath
        }))
      })

      it('understands coalesced creation and deletion events ^windows ^linux', async function () {
        const deletedPath = path.join(watchDir, 'deleted.txt')
        const recreatedPath = path.join(watchDir, 'recreated.txt')
        const createdPath = path.join(watchDir, 'created.txt')

        await fs.writeFile(deletedPath, 'initial contents\n')
        await until('file creation event arrives', eventMatching(
          {type: 'created', kind: 'file', oldPath: deletedPath}
        ))

        await fs.unlink(deletedPath)
        await fs.writeFile(recreatedPath, 'initial contents\n')
        await fs.unlink(recreatedPath)
        await fs.writeFile(recreatedPath, 'newly created\n')
        await fs.writeFile(createdPath, 'and another\n')

        await until('all events arrive', orderedEventsMatching(
          {type: 'deleted', kind: 'file', oldPath: deletedPath},
          {type: 'created', kind: 'file', oldPath: recreatedPath},
          {type: 'deleted', kind: 'file', oldPath: recreatedPath},
          {type: 'created', kind: 'file', oldPath: recreatedPath},
          {type: 'created', kind: 'file', oldPath: createdPath}
        ))
      })

      it('correlates rapid file rename events ^windows ^linux', async function () {
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
          {type: 'created', kind: 'file', oldPath: oldPath0},
          {type: 'created', kind: 'file', oldPath: oldPath1},
          {type: 'created', kind: 'file', oldPath: oldPath2}
        ))

        await Promise.all([
          fs.rename(oldPath0, newPath0),
          fs.rename(oldPath1, newPath1),
          fs.rename(oldPath2, newPath2)
        ])

        await until('all rename events arrive', allEventsMatching(
          {type: 'renamed', kind: 'file', oldPath: oldPath0, newPath: newPath0},
          {type: 'renamed', kind: 'file', oldPath: oldPath1, newPath: newPath1},
          {type: 'renamed', kind: 'file', oldPath: oldPath2, newPath: newPath2}
        ))
      })

      it('when a directory is created ^windows ^linux', async function () {
        const subdir = path.join(watchDir, 'subdir')
        await fs.mkdirs(subdir)

        await until('directory creation event arrives', eventMatching(
          {type: 'created', kind: 'directory', oldPath: subdir}
        ))
      })

      it('when a directory is renamed ^windows ^linux', async function () {
        const oldDir = path.join(watchDir, 'subdir')
        const newDir = path.join(watchDir, 'newdir')

        await fs.mkdirs(oldDir)
        await until('directory creation event arrives', eventMatching(
          {type: 'created', kind: 'directory', oldPath: oldDir}
        ))

        await fs.rename(oldDir, newDir)
        await until('directory rename event arrives', eventMatching(
          {type: 'renamed', kind: 'directory', oldPath: oldDir, newPath: newDir}
        ))
      })

      it('when a directory is deleted ^windows ^linux', async function () {
        const subdir = path.join(watchDir, 'subdir')
        await fs.mkdirs(subdir)
        await until('directory creation event arrives', eventMatching(
          {type: 'created', kind: 'directory', oldPath: subdir}
        ))

        await fs.rmdir(subdir)
        await until('directory deletion event arrives', eventMatching(
          {type: 'deleted', kind: 'directory', oldPath: subdir}
        ))
      })

      it('when a directory is deleted and a file is created in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        await fs.mkdir(reusedPath)
        await until('directory creation event arrives', eventMatching(
          {type: 'created', kind: 'directory', oldPath: reusedPath}
        ))

        await fs.rmdir(reusedPath)
        await fs.writeFile(reusedPath, 'IMMA FILE NOW, SURPRIIIISE\n')

        await until('deletion and creation events arrive', orderedEventsMatching(
          {type: 'deleted', kind: 'directory', oldPath: reusedPath},
          {type: 'created', kind: 'file', oldPath: reusedPath}
        ))
      })

      it('when a directory is deleted and a file is renamed in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        const oldFilePath = path.join(watchDir, 'oldfile')

        await Promise.all([
          fs.mkdirs(reusedPath),
          fs.writeFile(oldFilePath, 'original\n')
        ])
        await until('directory and file creation events arrive', allEventsMatching(
          {type: 'created', kind: 'directory', oldPath: reusedPath},
          {type: 'created', kind: 'file', oldPath: oldFilePath}
        ))

        await fs.rmdir(reusedPath)
        await fs.rename(oldFilePath, reusedPath)

        await until('deletion and rename events arrive', allEventsMatching(
          {type: 'deleted', kind: 'directory', oldPath: reusedPath},
          {type: 'renamed', kind: 'file', oldPath: oldFilePath, newPath: reusedPath}
        ))
      })

      it('when a directory is renamed and a file is created in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        const newDirPath = path.join(watchDir, 'newdir')

        await fs.mkdirs(reusedPath)
        await until('directory creation event arrives', eventMatching(
          {type: 'created', kind: 'directory', oldPath: reusedPath}
        ))

        await fs.rename(reusedPath, newDirPath)
        await fs.writeFile(reusedPath, 'oh look a file\n')

        await until('rename and creation events arrive', allEventsMatching(
          {type: 'renamed', kind: 'directory', oldPath: reusedPath, newPath: newDirPath},
          {type: 'created', kind: 'file', oldPath: reusedPath}
        ))
      })

      it('when a directory is renamed and a file is renamed in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        const oldFilePath = path.join(watchDir, 'oldfile')
        const newDirPath = path.join(watchDir, 'newdir')

        await Promise.all([
          fs.mkdirs(reusedPath),
          fs.writeFile(oldFilePath, 'started as a file\n')
        ])
        await until('directory and file creation evenst arrive', allEventsMatching(
          {type: 'created', kind: 'directory', oldPath: reusedPath},
          {type: 'created', kind: 'file', oldPath: oldFilePath}
        ))

        await fs.rename(reusedPath, newDirPath)
        await fs.rename(oldFilePath, reusedPath)

        await until('rename events arrive', allEventsMatching(
          {type: 'renamed', kind: 'directory', oldPath: reusedPath, newPath: newDirPath},
          {type: 'renamed', kind: 'file', oldPath: oldFilePath, newPath: reusedPath}
        ))
      })

      it('when a file is deleted and a directory is created in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        await fs.writeFile(reusedPath, 'something\n')
        await until('directory creation event arrives', eventMatching(
          {type: 'created', kind: 'file', oldPath: reusedPath}
        ))

        await fs.unlink(reusedPath)
        await fs.mkdir(reusedPath)

        await until('delete and create events arrive', orderedEventsMatching(
          {type: 'deleted', kind: 'file', oldPath: reusedPath},
          {type: 'created', kind: 'directory', oldPath: reusedPath}
        ))
      })

      it('when a file is deleted and a directory is renamed in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        const oldDirPath = path.join(watchDir, 'olddir')

        await Promise.all([
          fs.writeFile(reusedPath, 'something\n'),
          fs.mkdir(oldDirPath)
        ])
        await until('creation events arrive', allEventsMatching(
          {type: 'created', kind: 'file', oldPath: reusedPath},
          {type: 'created', kind: 'directory', oldPath: oldDirPath}
        ))

        await fs.unlink(reusedPath)
        await fs.rename(oldDirPath, reusedPath)

        await until('delete and rename events arrive', allEventsMatching(
          {type: 'deleted', kind: 'file', oldPath: reusedPath},
          {type: 'renamed', kind: 'directory', oldPath: oldDirPath, newPath: reusedPath}
        ))
      })

      it('when a file is renamed and a directory is created in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        const newFilePath = path.join(watchDir, 'newfile')

        await fs.writeFile(reusedPath, 'something\n')
        await until('directory creation event arrives', eventMatching(
          {type: 'created', kind: 'file', oldPath: reusedPath}
        ))

        await fs.rename(reusedPath, newFilePath)
        await fs.mkdir(reusedPath)

        await until('rename and create events arrive', orderedEventsMatching(
          {type: 'renamed', kind: 'file', oldPath: reusedPath, newPath: newFilePath},
          {type: 'created', kind: 'directory', oldPath: reusedPath}
        ))
      })

      it('when a file is renamed and a directory is renamed in its place ^windows ^linux', async function () {
        const reusedPath = path.join(watchDir, 'reused')
        const oldDirPath = path.join(watchDir, 'olddir')
        const newFilePath = path.join(watchDir, 'newfile')

        await Promise.all([
          fs.writeFile(reusedPath, 'something\n'),
          fs.mkdir(oldDirPath)
        ])
        await until('file and directory creation events arrive', allEventsMatching(
          {type: 'created', kind: 'file', oldPath: reusedPath},
          {type: 'created', kind: 'directory', oldPath: oldDirPath}
        ))

        await fs.rename(reusedPath, newFilePath)
        await fs.rename(oldDirPath, reusedPath)

        await until('rename events arrive', orderedEventsMatching(
          {type: 'renamed', kind: 'file', oldPath: reusedPath, newPath: newFilePath},
          {type: 'renamed', kind: 'directory', oldPath: oldDirPath, newPath: reusedPath}
        ))
      })
    })
  })

  describe('unwatching a directory', function () {
    beforeEach(async function () {
      if (!['darwin', 'win32'].includes(process.platform)) {
        this.skip()
      }

      await watcher.configure({mainLogFile, workerLogFile})
    })

    it('unwatches a previously watched directory ^windows ^linux', async function () {
      let error = null
      const events = []

      const sub = await watcher.watch(watchDir, (err, es) => {
        error = err
        events.push(...es)
      })
      subs.push(sub)

      const filePath = path.join(watchDir, 'file.txt')
      await fs.writeFile(filePath, 'original')

      await until('the event arrives', () => events.some(event => event.oldPath === filePath))
      const eventCount = events.length
      assert.isNull(error)

      await sub.unwatch()

      await fs.writeFile(filePath, 'the modification')

      // Give the modification event a chance to arrive.
      // Not perfect, but adequate.
      await new Promise(resolve => setTimeout(resolve, 100))

      assert.lengthOf(events, eventCount)
    })

    it('is a no-op if the directory is not being watched ^windows ^linux', async function () {
      let error = null
      const sub = await watcher.watch(watchDir, err => (error = err))
      subs.push(sub)
      assert.isNull(error)

      await sub.unwatch()
      assert.isNull(error)

      await sub.unwatch()
      assert.isNull(error)
    })
  })
})
