const fs = require('fs-extra')

const { Fixture } = require('../helper')
const { EventMatcher } = require('../matcher')

// These cases interfere with the caches on MacOS, but other platforms should handle them correctly as well.
describe('when a parent directory is renamed', function () {
  let fixture, matcher
  let originalParentDir, originalFile
  let finalParentDir, finalFile

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()

    originalParentDir = fixture.watchPath('parent-0')
    originalFile = fixture.watchPath('parent-0', 'file.txt')
    finalParentDir = fixture.watchPath('parent-1')
    finalFile = fixture.watchPath('parent-1', 'file.txt')

    await fs.mkdir(originalParentDir)
    await fs.writeFile(originalFile, 'contents\n')

    matcher = new EventMatcher(fixture)
    await matcher.watch([], {})
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  it('tracks the file rename across event batches', async function () {
    const changedFile = fixture.watchPath('parent-1', 'file-1.txt')

    await fs.rename(originalParentDir, finalParentDir)
    await until('the rename event arrives', matcher.allEvents(
      { action: 'renamed', kind: 'directory', oldPath: originalParentDir, path: finalParentDir }
    ))

    await fs.rename(finalFile, changedFile)

    await until('the rename event arrives', matcher.allEvents(
      { action: 'renamed', kind: 'file', oldPath: finalFile, path: changedFile }
    ))
  })

  it('tracks the file rename within the same event batch', async function () {
    const changedFile = fixture.watchPath('parent-1', 'file-1.txt')

    await fs.rename(originalParentDir, finalParentDir)
    await fs.rename(finalFile, changedFile)

    await until('the rename events arrive', matcher.allEvents(
      { action: 'renamed', kind: 'directory', oldPath: originalParentDir, path: finalParentDir },
      { action: 'renamed', kind: 'file', oldPath: finalFile, path: changedFile }
    ))
  })

  it('tracks the file rename when the file is renamed first', async function () {
    const changedFile = fixture.watchPath('parent-0', 'file-1.txt')

    await fs.rename(originalFile, changedFile)
    await fs.rename(originalParentDir, finalParentDir)

    await until('the rename events arrive', matcher.allEvents(
      { action: 'renamed', kind: 'file', oldPath: originalFile, path: changedFile },
      { action: 'renamed', kind: 'directory', oldPath: originalParentDir, path: finalParentDir }
    ))
  })
})
