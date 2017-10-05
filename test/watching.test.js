const fs = require('fs-extra')
const path = require('path')

const watcher = require('../lib')

const {prepareFixtureDir, reportLogs, cleanupFixtureDir} = require('./helper')

describe('watching a directory', function () {
  let subs, fixtureDir, watchDir, mainLogFile, workerLogFile, pollingLogFile

  beforeEach(async function () {
    subs = [];
    ({fixtureDir, watchDir, mainLogFile, workerLogFile, pollingLogFile} = await prepareFixtureDir())

    await watcher.configure({mainLog: mainLogFile, workerLog: workerLogFile, pollingLog: pollingLogFile})
  })

  afterEach(async function () {
    await Promise.all(subs.map(sub => sub.unwatch()))
    await reportLogs(this.currentTest, mainLogFile, workerLogFile, pollingLogFile)
    await cleanupFixtureDir(fixtureDir)
  })

  it('begins receiving events within that directory', async function () {
    let error = null
    const events = []

    subs.push(await watcher.watch(watchDir, {}, (err, es) => {
      error = err
      events.push(...es)
    }))

    await fs.writeFile(path.join(watchDir, 'file.txt'), 'indeed')

    await until('an event arrives', () => events.length > 0)
    assert.isNull(error)
  })

  it('can watch multiple directories at once and dispatch events appropriately', async function () {
    const errors = []
    const eventsA = []
    const eventsB = []

    const watchDirA = path.join(watchDir, 'dir_a')
    const watchDirB = path.join(watchDir, 'dir_b')
    await Promise.all(
      [watchDirA, watchDirB].map(subdir => fs.mkdir(subdir))
    )

    subs.push(await watcher.watch(watchDirA, {}, (err, es) => {
      errors.push(err)
      eventsA.push(...es)
    }))
    subs.push(await watcher.watch(watchDirB, {}, (err, es) => {
      errors.push(err)
      eventsB.push(...es)
    }))

    const fileA = path.join(watchDirA, 'a.txt')
    await fs.writeFile(fileA, 'file a')
    await until('watcher A picks up its event', () => eventsA.some(event => event.path === fileA))

    const fileB = path.join(watchDirB, 'b.txt')
    await fs.writeFile(fileB, 'file b')
    await until('watcher B picks up its event', () => eventsB.some(event => event.path === fileB))

    // Assert that the streams weren't crossed
    assert.isTrue(errors.every(err => err === null))
    assert.isTrue(eventsA.every(event => event.oldPath !== fileB))
    assert.isTrue(eventsB.every(event => event.oldPath !== fileA))
  })

  it('watches subdirectories recursively', async function () {
    const errors = []
    const events = []

    const subdir0 = path.join(watchDir, 'subdir0')
    const subdir1 = path.join(watchDir, 'subdir1')
    await Promise.all(
      [subdir0, subdir1].map(subdir => fs.mkdir(subdir))
    )

    subs.push(await watcher.watch(watchDir, {}, (err, es) => {
      errors.push(err)
      events.push(...es)
    }))

    const rootFile = path.join(watchDir, 'root.txt')
    await fs.writeFile(rootFile, 'root')

    const file0 = path.join(subdir0, '0.txt')
    await fs.writeFile(file0, 'file 0')

    const file1 = path.join(subdir1, '1.txt')
    await fs.writeFile(file1, 'file 1')

    await until('all three events arrive', () => {
      return [rootFile, file0, file1].every(filePath => events.some(event => event.path === filePath))
    })
    assert.isTrue(errors.every(err => err === null))
  })

  it('watches newly created subdirectories', async function () {
    const errors = []
    const events = []

    subs.push(await watcher.watch(watchDir, {}, (err, es) => {
      errors.push(err)
      events.push(...es)
    }))

    const subdir = path.join(watchDir, 'subdir')
    const file0 = path.join(subdir, 'file-0.txt')

    await fs.mkdir(subdir)
    await until('the subdirectory creation event arrives', () => {
      return events.some(event => event.path === subdir)
    })

    await fs.writeFile(file0, 'file 0')
    await until('the modification event arrives', () => {
      return events.some(event => event.path === file0)
    })
    assert.isTrue(errors.every(err => err === null))
  })

  it('watches directories renamed within a watch root', async function () {
    const errors = []
    const events = []

    const externalDir = path.join(fixtureDir, 'outside')
    const externalSubdir = path.join(externalDir, 'directory')
    const externalFile = path.join(externalSubdir, 'file.txt')

    const internalDir = path.join(watchDir, 'inside')
    const internalSubdir = path.join(internalDir, 'directory')
    const internalFile = path.join(internalSubdir, 'file.txt')

    await fs.mkdirs(externalSubdir)
    await fs.writeFile(externalFile, 'contents')

    subs.push(await watcher.watch(watchDir, {}, (err, es) => {
      errors.push(err)
      events.push(...es)
    }))

    await fs.rename(externalDir, internalDir)
    await until('creation event arrives', () => {
      return events.some(event => event.path === internalDir)
    })

    await fs.writeFile(internalFile, 'changed')

    await until('modification event arrives', () => {
      return events.some(event => event.path === internalFile)
    })
    assert.isTrue(errors.every(err => err === null))
  })
})
