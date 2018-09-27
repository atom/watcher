/* eslint-dev mocha */
const fs = require('fs-extra')

const { Fixture } = require('./helper')
const { EventMatcher } = require('./matcher')

describe('exported functions', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  describe('watchPath()', function () {
    it('resolves the returned promise when the watcher begins listening', async function () {
      const matcher = new EventMatcher(fixture)

      const watcher = await matcher.watch([], {})
      assert.strictEqual(watcher.constructor.name, 'PathWatcher')
    })

    it('reuses an existing native watcher and resolves getStartPromise immediately if attached to a running watcher', async function () {
      const matcher = new EventMatcher(fixture)
      const watcher0 = await matcher.watch([], {})
      const watcher1 = await matcher.watch([], {})

      assert.strictEqual(watcher0.native, watcher1.native)
    })

    it("reuses existing native watchers even while they're still starting", async function () {
      const matcher = new EventMatcher(fixture)
      const [watcher0, watcher1] = await Promise.all([
        matcher.watch([], {}),
        matcher.watch([], {})
      ])
      assert.strictEqual(watcher0.native, watcher1.native)
    })

    it("doesn't attach new watchers to a native watcher that's stopping", async function () {
      const matcher = new EventMatcher(fixture)
      const watcher0 = await matcher.watch([], {})
      const native0 = watcher0.native

      watcher0.dispose()
      const watcher1 = await matcher.watch([], {})

      assert.notStrictEqual(watcher1.native, native0)
    })

    it('reuses an existing native watcher on a parent directory and filters events', async function () {
      const rootFile = fixture.watchPath('rootfile.txt')
      const subDir = fixture.watchPath('subdir')
      const subFile = fixture.watchPath('subdir', 'subfile.txt')

      await fs.mkdir(subDir)

      // Keep the watchers alive with an undisposed subscription
      const rootMatcher = new EventMatcher(fixture)
      const rootWatcher = await rootMatcher.watch([], {})
      const childMatcher = new EventMatcher(fixture)
      const childWatcher = await childMatcher.watch(['subdir'], {})

      assert.strictEqual(rootWatcher.native, childWatcher.native)
      assert.isTrue(rootWatcher.native.isRunning())

      await fs.writeFile(subFile, 'subfile\n', { encoding: 'utf8' })

      await Promise.all([
        until('root events arrive', rootMatcher.allEvents({ path: subFile })),
        until('child events arrive', childMatcher.allEvents({ path: subFile }))
      ])

      await fs.writeFile(rootFile, 'rootfile\n', { encoding: 'utf8' })
      await until('root event arrives', rootMatcher.allEvents({ path: rootFile }))
      assert.isTrue(childMatcher.noEvents({ path: rootFile }))
    })

    it('adopts existing child watchers and filters events appropriately to them', async function () {
      // Create the directory tree
      const rootFile = fixture.watchPath('rootfile.txt')
      const subDir0 = fixture.watchPath('subdir0')
      const subFile0 = fixture.watchPath('subdir0', 'subfile0.txt')
      const subDir1 = fixture.watchPath('subdir1')
      const subFile1 = fixture.watchPath('subdir1', 'subfile1.txt')

      await fs.mkdir(subDir0)
      await fs.mkdir(subDir1)
      await Promise.all([
        fs.writeFile(rootFile, 'rootfile\n', { encoding: 'utf8' }),
        fs.writeFile(subFile0, 'subfile 0\n', { encoding: 'utf8' }),
        fs.writeFile(subFile1, 'subfile 1\n', { encoding: 'utf8' })
      ])

      // Begin the child watchers
      const subMatcher0 = new EventMatcher(fixture)
      const subWatcher0 = await subMatcher0.watch(['subdir0'], {})

      const subMatcher1 = new EventMatcher(fixture)
      const subWatcher1 = await subMatcher1.watch(['subdir1'], {})

      assert.notStrictEqual(subWatcher0.native, subWatcher1.native)

      // Create the parent watcher
      const parentMatcher = new EventMatcher(fixture)
      const parentWatcher = await parentMatcher.watch([], {})

      assert.strictEqual(subWatcher0.native, parentWatcher.native)
      assert.strictEqual(subWatcher1.native, parentWatcher.native)

      // Ensure events are filtered correctly
      await Promise.all([
        fs.appendFile(rootFile, 'change\n', { encoding: 'utf8' }),
        fs.appendFile(subFile0, 'change\n', { encoding: 'utf8' }),
        fs.appendFile(subFile1, 'change\n', { encoding: 'utf8' })
      ])

      await Promise.all([
        until('subwatcher 0 changes', subMatcher0.allEvents({ path: subFile0 })),
        until('subwatcher 1 changes', subMatcher1.allEvents({ path: subFile1 })),
        until('parent changes', parentMatcher.allEvents(
          { path: rootFile },
          { path: subFile0 },
          { path: subFile1 }
        ))
      ])

      assert.isTrue(subMatcher0.noEvents({ path: subFile1 }, { path: rootFile }))
      assert.isTrue(subMatcher1.noEvents({ path: subFile0 }, { path: rootFile }))
    })

    describe('{recursive: false}', function () {
      let immediateFile, immediateSubdir, subdirFile, deepSubdir, deepFile

      beforeEach(async function () {
        immediateFile = fixture.watchPath('immediate-file.txt')
        immediateSubdir = fixture.watchPath('subdir0')
        subdirFile = fixture.watchPath('subdir0', 'subdir-file.txt')
        deepSubdir = fixture.watchPath('subdir0', 'subdir1')
        deepFile = fixture.watchPath('subdir0', 'subdir1', 'deep-file.txt')

        await fs.mkdir(immediateSubdir)
        await fs.mkdir(deepSubdir)
        await Promise.all([
          fs.writeFile(immediateFile, 'contents\n'),
          fs.writeFile(subdirFile, 'contents\n'),
          fs.writeFile(deepFile, 'contents\n')
        ])
      })

      it('only receives events for immediate children', async function () {
        const matcher = new EventMatcher(fixture)

        await matcher.watch([], { recursive: false })

        await fs.appendFile(subdirFile, 'newline\n')
        await fs.appendFile(immediateFile, 'newline\n')

        await until('immediate event arrives', matcher.allEvents({ path: immediateFile }))
        assert.isTrue(matcher.noEvents({ path: subdirFile }))
      })

      it('attaches to an existing non-recursive watcher at the same path', async function () {
        const matcher0 = new EventMatcher(fixture)
        const watcher0 = await matcher0.watch([], { recursive: false })

        const matcher1 = new EventMatcher(fixture)
        const watcher1 = await matcher1.watch([], { recursive: false })

        assert.strictEqual(watcher0.native, watcher1.native)

        await fs.writeFile(subdirFile, 'newline\n')
        await fs.writeFile(immediateFile, 'newline\n')

        await until('immediate event arrives on matcher 0', matcher0.allEvents({ path: immediateFile }))
        await until('immediate event arrives on matcher 1', matcher1.allEvents({ path: immediateFile }))

        assert.isTrue(matcher0.noEvents({ path: subdirFile }))
        assert.isTrue(matcher1.noEvents({ path: subdirFile }))
      })

      it('attaches to an existing recursive watcher and filters events', async function () {
        const matcher0 = new EventMatcher(fixture)
        const watcher0 = await matcher0.watch([], { recursive: true })

        const matcher1 = new EventMatcher(fixture)
        const watcher1 = await matcher1.watch(['subdir0'], { recursive: false })

        assert.strictEqual(watcher1.native, watcher0.native)

        await fs.writeFile(subdirFile, 'newline\n')
        await fs.writeFile(immediateFile, 'newline\n')
        await fs.writeFile(deepFile, 'newline\n')

        await until('both events arrive on matcher 0', matcher0.allEvents(
          { path: immediateFile },
          { path: subdirFile },
          { path: deepFile }
        ))
        await until('immediate event arrives on matcher 1', matcher1.allEvents(
          { path: subdirFile }
        ))

        assert.isTrue(matcher1.noEvents(
          { path: immediateFile },
          { path: deepFile }
        ))
      })
    })

    describe('single file', function () {
      let watchedFile, unwatchedFile, matcher

      beforeEach(async function () {
        watchedFile = fixture.watchPath('watched.txt')
        unwatchedFile = fixture.watchPath('unwatched.txt')

        await Promise.all([
          fs.writeFile(watchedFile, 'watched\n'),
          fs.writeFile(unwatchedFile, 'unwatched\n')
        ])

        matcher = new EventMatcher(fixture)
        await matcher.watch(['watched.txt'], {})
      })

      it('receives events for only that file', async function () {
        await fs.appendFile(unwatchedFile, 'nope\n')
        await fs.appendFile(watchedFile, 'yep\n')

        await until(matcher.allEvents({ path: watchedFile }))
        assert.isTrue(matcher.noEvents({ path: unwatchedFile }))
      })

      it('continues to work after moving the file away and creating a new one in its place', async function () {
        let newName = fixture.watchPath('new-name.txt')

        await fs.rename(watchedFile, newName)
        await until('deletion event arrives', matcher.allEvents({ path: watchedFile, action: 'deleted' }))

        await fs.writeFile(watchedFile, 'indeed\n')
        await until('creation event arrives', matcher.allEvents({ path: watchedFile, action: 'created' }))

        await fs.appendFile(newName, 'nope\n')
        await fs.appendFile(unwatchedFile, 'nope\n')
        await fs.appendFile(watchedFile, 'yep\n')

        await until('modification event arrives', matcher.allEvents({ path: watchedFile, action: 'modified' }))

        assert.isTrue(matcher.noEvents({ path: unwatchedFile }))
        assert.isTrue(matcher.noEvents({ path: newName }))
      })
    })
  })
})
