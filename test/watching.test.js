const fs = require('fs-extra')
const { Fixture } = require('./helper')
const { EventMatcher } = require('./matcher')

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
    const matcher = new EventMatcher(fixture)
    await matcher.watch([], {})

    const filePath = fixture.watchPath('file.txt')
    await fs.writeFile(filePath, 'indeed')

    await until('an event arrives', matcher.allEvents({ path: filePath }))
  })

  it('can watch multiple directories at once and dispatch events appropriately', async function () {
    const watchDirA = fixture.watchPath('dir_a')
    const watchDirB = fixture.watchPath('dir_b')
    await Promise.all(
      [watchDirA, watchDirB].map(subdir => fs.mkdir(subdir))
    )

    const matcherA = new EventMatcher(fixture)
    await matcherA.watch(['dir_a'], {})

    const matcherB = new EventMatcher(fixture)
    await matcherB.watch(['dir_b'], {})

    const fileA = fixture.watchPath('dir_a', 'a.txt')
    await fs.writeFile(fileA, 'file a')
    await until('watcher A picks up its event', matcherA.allEvents({ path: fileA }))

    const fileB = fixture.watchPath('dir_b', 'b.txt')
    await fs.writeFile(fileB, 'file b')
    await until('watcher B picks up its event', matcherB.allEvents({ path: fileB }))

    // Assert that the streams weren't crossed
    assert.isTrue(matcherA.noEvents({ path: fileB }))
    assert.isTrue(matcherB.noEvents({ path: fileA }))
  })

  it('watches subdirectories recursively', async function () {
    const subdir0 = fixture.watchPath('subdir0')
    const subdir1 = fixture.watchPath('subdir1')
    await Promise.all(
      [subdir0, subdir1].map(subdir => fs.mkdir(subdir))
    )

    const matcher = new EventMatcher(fixture)
    await matcher.watch([], {})

    const rootFile = fixture.watchPath('root.txt')
    await fs.writeFile(rootFile, 'root')

    const file0 = fixture.watchPath('subdir0', '0.txt')
    await fs.writeFile(file0, 'file 0')

    const file1 = fixture.watchPath('subdir1', '1.txt')
    await fs.writeFile(file1, 'file 1')

    await until('all three events arrive', matcher.allEvents(
      { path: rootFile },
      { path: file0 },
      { path: file1 }
    ))
  })

  it('watches newly created subdirectories', async function () {
    const matcher = new EventMatcher(fixture)
    await matcher.watch([], {})

    const subdir = fixture.watchPath('subdir')
    const file0 = fixture.watchPath('subdir', 'file-0.txt')

    await fs.mkdir(subdir)
    await until('the subdirectory creation event arrives', matcher.allEvents({ path: subdir }))

    await fs.writeFile(file0, 'file 0')
    await until('the modification event arrives', matcher.allEvents({ path: file0 }))
  })

  it('watches directories renamed within a watch root', async function () {
    const externalDir = fixture.fixturePath('outside')
    const externalSubdir = fixture.fixturePath('outside', 'directory')
    const externalFile = fixture.fixturePath('outside', 'directory', 'file.txt')

    const internalDir = fixture.watchPath('inside')
    const internalFile = fixture.watchPath('inside', 'directory', 'file.txt')

    await fs.mkdirs(externalSubdir)
    await fs.writeFile(externalFile, 'contents')

    const matcher = new EventMatcher(fixture)
    await matcher.watch([], {})

    await fs.rename(externalDir, internalDir)
    await until('creation event arrives', matcher.allEvents({ path: internalDir }))

    await fs.writeFile(internalFile, 'changed')

    await until('modification event arrives', matcher.allEvents({ path: internalFile }))
  })

  it('can watch a directory nested within an already-watched directory', async function () {
    const rootFile = fixture.watchPath('root-file.txt')
    const subDir = fixture.watchPath('subdir')
    const subFile = fixture.watchPath('subdir', 'sub-file.txt')

    await fs.mkdir(subDir)
    await fs.writeFile(rootFile, 'root\n')
    await fs.writeFile(subFile, 'sub\n')

    const parent = new EventMatcher(fixture)
    await parent.watch([], {})

    const child = new EventMatcher(fixture)
    const w = await child.watch(['subdir'], {})

    await fs.appendFile(rootFile, 'change 0\n')
    await fs.appendFile(subFile, 'change 0\n')

    await until('parent events arrive', parent.allEvents(
      { path: rootFile },
      { path: subFile }
    ))
    await until('child events arrive', parent.allEvents(
      { path: subFile }
    ))

    w.dispose()
    parent.reset()

    await fs.appendFile(rootFile, 'change 1\n')
    await fs.appendFile(subFile, 'change 1\n')

    await until('parent events arrive', parent.allEvents(
      { path: rootFile },
      { path: subFile }
    ))
  })
})
