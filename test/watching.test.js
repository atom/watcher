const fs = require('fs-extra')
const {Fixture} = require('./helper')

describe('watching a directory', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  it('begins receiving events within that directory', async function () {
    let error = null
    const events = []

    await fixture.watch([], {}, (err, es) => {
      error = err
      events.push(...es)
    })

    await fs.writeFile(fixture.watchPath('file.txt'), 'indeed')

    await until('an event arrives', () => events.length > 0)
    assert.isNull(error)
  })

  it('can watch multiple directories at once and dispatch events appropriately', async function () {
    const errors = []
    const eventsA = []
    const eventsB = []

    const watchDirA = fixture.watchPath('dir_a')
    const watchDirB = fixture.watchPath('dir_b')
    await Promise.all(
      [watchDirA, watchDirB].map(subdir => fs.mkdir(subdir))
    )

    await fixture.watch(['dir_a'], {}, (err, es) => {
      errors.push(err)
      eventsA.push(...es)
    })
    await fixture.watch(['dir_b'], {}, (err, es) => {
      errors.push(err)
      eventsB.push(...es)
    })

    const fileA = fixture.watchPath('dir_a', 'a.txt')
    await fs.writeFile(fileA, 'file a')
    await until('watcher A picks up its event', () => eventsA.some(event => event.path === fileA))

    const fileB = fixture.watchPath('dir_b', 'b.txt')
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

    const subdir0 = fixture.watchPath('subdir0')
    const subdir1 = fixture.watchPath('subdir1')
    await Promise.all(
      [subdir0, subdir1].map(subdir => fs.mkdir(subdir))
    )

    await fixture.watch([], {}, (err, es) => {
      errors.push(err)
      events.push(...es)
    })

    const rootFile = fixture.watchPath('root.txt')
    await fs.writeFile(rootFile, 'root')

    const file0 = fixture.watchPath('subdir0', '0.txt')
    await fs.writeFile(file0, 'file 0')

    const file1 = fixture.watchPath('subdir1', '1.txt')
    await fs.writeFile(file1, 'file 1')

    await until('all three events arrive', () => {
      return [rootFile, file0, file1].every(filePath => events.some(event => event.path === filePath))
    })
    assert.isTrue(errors.every(err => err === null))
  })

  it('watches newly created subdirectories', async function () {
    const errors = []
    const events = []

    await fixture.watch([], {}, (err, es) => {
      errors.push(err)
      events.push(...es)
    })

    const subdir = fixture.watchPath('subdir')
    const file0 = fixture.watchPath('subdir', 'file-0.txt')

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

    const externalDir = fixture.fixturePath('outside')
    const externalSubdir = fixture.fixturePath('outside', 'directory')
    const externalFile = fixture.fixturePath('outside', 'directory', 'file.txt')

    const internalDir = fixture.watchPath('inside')
    const internalFile = fixture.watchPath('inside', 'directory', 'file.txt')

    await fs.mkdirs(externalSubdir)
    await fs.writeFile(externalFile, 'contents')

    await fixture.watch([], {}, (err, es) => {
      errors.push(err)
      events.push(...es)
    })

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
