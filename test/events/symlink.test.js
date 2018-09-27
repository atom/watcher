const fs = require('fs-extra')

const { Fixture } = require('../helper')
const { EventMatcher } = require('../matcher')

describe('watching beneath symlinked directories', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  it('reports paths consistently with the argument to watchPath', async function () {
    const realSubdir = fixture.watchPath('realdir')
    const realFile = fixture.watchPath('realdir', 'file.txt')

    const symlinkSubdir = fixture.watchPath('linkdir')
    const symlinkFile = fixture.watchPath('linkdir', 'file.txt')

    await fs.mkdirs(realSubdir)
    await fs.symlink(realSubdir, symlinkSubdir)

    const symlinkMatcher = new EventMatcher(fixture)
    const sw = await symlinkMatcher.watch(['linkdir'], {})

    const realMatcher = new EventMatcher(fixture)
    const rw = await realMatcher.watch(['realdir'], {})

    assert.strictEqual(sw.native, rw.native)

    await fs.writeFile(realFile, 'contents\n')

    await Promise.all([
      until('symlink event arrives', symlinkMatcher.allEvents({ action: 'created', kind: 'file', path: symlinkFile })),
      until('real path event arrives', realMatcher.allEvents({ action: 'created', kind: 'file', path: realFile }))
    ])
  })

  it("doesn't die horribly on a symlink loop", async function () {
    const parentDir = fixture.watchPath('parent')
    const subDir = fixture.watchPath('parent', 'child')
    const linkName = fixture.watchPath('parent', 'child', 'turtles')
    const fileName = fixture.watchPath('parent', 'file.txt')

    await fs.mkdirs(subDir)
    await fs.symlink(parentDir, linkName)

    const matcher = new EventMatcher(fixture)
    await matcher.watch([''], {})

    await fs.writeFile(fileName, 'contents\n')

    await until('creation event arrives', matcher.allEvents(
      { action: 'created', kind: 'file', path: fileName }
    ))
  })

  it("doesn't die horribly on a broken symlink", async function () {
    const targetName = fixture.watchPath('target.txt')
    const linkName = fixture.watchPath('symlink.txt')
    const fileName = fixture.watchPath('file.txt')

    await fs.writeFile(targetName, 'original\n')
    await fs.symlink(targetName, linkName)
    await fs.unlink(targetName)

    const matcher = new EventMatcher(fixture)
    await matcher.watch([''], {})

    await fs.writeFile(fileName, 'stuff\n')

    await until('creation event arrives', matcher.allEvents(
      { action: 'created', kind: 'file', path: fileName }
    ))
  })
})
