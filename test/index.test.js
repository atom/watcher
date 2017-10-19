const temp = require('temp')
const fs = require('fs-extra')
const path = require('path')

const {CompositeDisposable} = require('event-kit')
const {Fixture} = require('./helper')
const {EventMatcher} = require('./matcher')
const {watchPath, stopAllWatchers} = require('../lib')

let tempDirs = []

describe('exported functions', function () {
  let subs

  beforeEach(function () {
    subs = new CompositeDisposable()
  })

  afterEach(async function () {
    subs.dispose()

    await stopAllWatchers()

    await Promise.all(
      tempDirs.map(tempDir => {
        return fs.remove(tempDir, {maxBusyTries: 1})
          .catch(err => { console.warn('Unable to delete fixture directory', err) })
      })
    )
    tempDirs = []
  })

  function tempMkdir (...args) {
    return new Promise((resolve, reject) => {
      temp.mkdir(...args, (err, dirPath) => {
        if (err) {
          reject(err)
        } else {
          tempDirs.push(dirPath)
          resolve(dirPath)
        }
      })
    })
  }

  function waitForChanges (watcher, ...fileNames) {
    const waiting = new Set(fileNames)
    let fired = false
    const relevantEvents = []

    return new Promise(resolve => {
      const sub = watcher.onDidChange(events => {
        for (const event of events) {
          if (waiting.delete(event.path)) {
            relevantEvents.push(event)
          }
        }

        if (!fired && waiting.size === 0) {
          fired = true
          resolve(relevantEvents)
          sub.dispose()
        }
      })
    })
  }

  describe('watchPath()', function () {
    it('resolves the returned promise when the watcher begins listening', async function () {
      const rootDir = await tempMkdir('atom-watcher-test-')

      const watcher = await watchPath(rootDir, {}, () => {})
      assert.strictEqual(watcher.constructor.name, 'PathWatcher')
    })

    it('reuses an existing native watcher and resolves getStartPromise immediately if attached to a running watcher', async function () {
      const rootDir = await tempMkdir('atom-watcher-test-')

      const watcher0 = await watchPath(rootDir, {}, () => {})
      const watcher1 = await watchPath(rootDir, {}, () => {})

      assert.strictEqual(watcher0.native, watcher1.native)
    })

    it("reuses existing native watchers even while they're still starting", async function () {
      const rootDir = await tempMkdir('atom-watcher-test-')

      const [watcher0, watcher1] = await Promise.all([
        watchPath(rootDir, {}, () => {}),
        watchPath(rootDir, {}, () => {})
      ])
      assert.strictEqual(watcher0.native, watcher1.native)
    })

    it("doesn't attach new watchers to a native watcher that's stopping", async function () {
      const rootDir = await tempMkdir('atom-watcher-test-')

      const watcher0 = await watchPath(rootDir, {}, () => {})
      const native0 = watcher0.native

      watcher0.dispose()
      const watcher1 = await watchPath(rootDir, {}, () => {})

      assert.notStrictEqual(watcher1.native, native0)
    })

    it('reuses an existing native watcher on a parent directory and filters events', async function () {
      const rootDir = await tempMkdir('atom-watcher-test-').then(fs.realpath)
      const rootFile = path.join(rootDir, 'rootfile.txt')
      const subDir = path.join(rootDir, 'subdir')
      const subFile = path.join(subDir, 'subfile.txt')

      await fs.mkdir(subDir)

      // Keep the watchers alive with an undisposed subscription
      const rootWatcher = await watchPath(rootDir, {}, () => {})
      const childWatcher = await watchPath(subDir, {}, () => {})

      assert.strictEqual(rootWatcher.native, childWatcher.native)
      assert.isTrue(rootWatcher.native.isRunning())

      const firstChanges = Promise.all([
        waitForChanges(rootWatcher, subFile),
        waitForChanges(childWatcher, subFile)
      ])
      await fs.writeFile(subFile, 'subfile\n', {encoding: 'utf8'})
      await firstChanges

      const nextRootEvent = waitForChanges(rootWatcher, rootFile)
      await fs.writeFile(rootFile, 'rootfile\n', {encoding: 'utf8'})
      await nextRootEvent
    })

    it('adopts existing child watchers and filters events appropriately to them', async function () {
      const parentDir = await tempMkdir('atom-watcher-test-').then(fs.realpath)

      // Create the directory tree
      const rootFile = path.join(parentDir, 'rootfile.txt')
      const subDir0 = path.join(parentDir, 'subdir0')
      const subFile0 = path.join(subDir0, 'subfile0.txt')
      const subDir1 = path.join(parentDir, 'subdir1')
      const subFile1 = path.join(subDir1, 'subfile1.txt')

      await fs.mkdir(subDir0)
      await fs.mkdir(subDir1)
      await Promise.all([
        fs.writeFile(rootFile, 'rootfile\n', {encoding: 'utf8'}),
        fs.writeFile(subFile0, 'subfile 0\n', {encoding: 'utf8'}),
        fs.writeFile(subFile1, 'subfile 1\n', {encoding: 'utf8'})
      ])

      // Begin the child watchers and keep them alive
      const subWatcher0 = await watchPath(subDir0, {}, () => {})
      const subWatcherChanges0 = waitForChanges(subWatcher0, subFile0)

      const subWatcher1 = await watchPath(subDir1, {}, () => {})
      const subWatcherChanges1 = waitForChanges(subWatcher1, subFile1)

      assert.notStrictEqual(subWatcher0.native, subWatcher1.native)

      // Create the parent watcher
      const parentWatcher = await watchPath(parentDir, {}, () => {})
      const parentWatcherChanges = waitForChanges(parentWatcher, rootFile, subFile0, subFile1)

      assert.strictEqual(subWatcher0.native, parentWatcher.native)
      assert.strictEqual(subWatcher1.native, parentWatcher.native)

      // Ensure events are filtered correctly
      await Promise.all([
        fs.appendFile(rootFile, 'change\n', {encoding: 'utf8'}),
        fs.appendFile(subFile0, 'change\n', {encoding: 'utf8'}),
        fs.appendFile(subFile1, 'change\n', {encoding: 'utf8'})
      ])

      await Promise.all([
        subWatcherChanges0,
        subWatcherChanges1,
        parentWatcherChanges
      ])
    })

    describe('{recursive: false}', function () {
      let fixture
      let immediateFile, immediateSubdir, subdirFile, deepSubdir, deepFile

      beforeEach(async function () {
        fixture = new Fixture()
        fixture.createWatcherWith(watchPath)
        await fixture.before()
        await fixture.log()

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

      afterEach(async function () {
        await fixture.after(this.currentTest)
      })

      it('only receives events for immediate children', async function () {
        const matcher = new EventMatcher(fixture)

        await matcher.watch([], {recursive: false})

        await fs.appendFile(subdirFile, 'newline\n')
        await fs.appendFile(immediateFile, 'newline\n')

        await until('immediate event arrives', matcher.allEvents({path: immediateFile}))
        assert.isTrue(matcher.noEvents({path: subdirFile}))
      })

      it('attaches to an existing non-recursive watcher at the same path', async function () {
        const matcher0 = new EventMatcher(fixture)
        const watcher0 = await matcher0.watch([], {recursive: false})

        const matcher1 = new EventMatcher(fixture)
        const watcher1 = await matcher1.watch([], {recursive: false})

        assert.strictEqual(watcher0.native, watcher1.native)

        await fs.writeFile(subdirFile, 'newline\n')
        await fs.writeFile(immediateFile, 'newline\n')

        await until('immediate event arrives on matcher 0', matcher0.allEvents({path: immediateFile}))
        await until('immediate event arrives on matcher 1', matcher1.allEvents({path: immediateFile}))

        assert.isTrue(matcher0.noEvents({path: subdirFile}))
        assert.isTrue(matcher1.noEvents({path: subdirFile}))
      })

      it('attaches to an existing recursive watcher and filters events', async function () {
        const matcher0 = new EventMatcher(fixture)
        const watcher0 = await matcher0.watch([], {recursive: true})

        const matcher1 = new EventMatcher(fixture)
        const watcher1 = await matcher1.watch(['subdir0'], {recursive: false})

        assert.strictEqual(watcher1.native, watcher0.native)

        await fs.writeFile(subdirFile, 'newline\n')
        await fs.writeFile(immediateFile, 'newline\n')
        await fs.writeFile(deepFile, 'newline\n')

        await until('both events arrive on matcher 0', matcher0.allEvents(
          {path: immediateFile},
          {path: subdirFile},
          {path: deepFile}
        ))
        await until('immediate event arrives on matcher 1', matcher1.allEvents(
          {path: subdirFile}
        ))

        assert.isTrue(matcher1.noEvents(
          {path: immediateFile},
          {path: deepFile}
        ))
      })
    })
  })
})
