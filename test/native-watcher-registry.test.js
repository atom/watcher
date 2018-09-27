const path = require('path')
const { Emitter } = require('event-kit')

const { NativeWatcherRegistry } = require('../lib/native-watcher-registry')

function findRootDirectory () {
  let current = process.cwd()
  while (true) {
    let next = path.resolve(current, '..')
    if (next === current) {
      return next
    } else {
      current = next
    }
  }
}
const ROOT = findRootDirectory()

function absolute (...parts) {
  const candidate = path.join(...parts)
  return path.isAbsolute(candidate) ? candidate : path.join(ROOT, candidate)
}

function parts (fullPath) {
  return fullPath.split(path.sep).filter(part => part.length > 0)
}

class MockWatcher {
  constructor (normalizedPath, options) {
    this.normalizedPath = normalizedPath
    this.options = Object.assign({ recursive: true }, options)
    this.native = null
  }

  getOptions () {
    return this.options
  }

  getNormalizedPathPromise () {
    return Promise.resolve(this.normalizedPath)
  }

  attachToNative (native, nativePath, options) {
    if (this.native === native) return
    if (!this.normalizedPath.startsWith(nativePath)) return
    if (!options.recursive && this.normalizedPath !== nativePath) return

    if (this.native) {
      this.native.attached = this.native.attached.filter(each => each !== this)
    }
    this.native = native
    this.native.attached.push(this)
  }
}

class MockNative {
  constructor (name) {
    this.name = name
    this.attached = []
    this.stopped = false

    this.emitter = new Emitter()
  }

  reattachTo (newNative, nativePath, options) {
    for (const watcher of this.attached) {
      watcher.attachToNative(newNative, nativePath, options)
    }
  }

  onWillStop (callback) {
    return this.emitter.on('will-stop', callback)
  }

  stop (split = true) {
    this.stopped = true
    this.emitter.emit('will-stop', split)
  }
}

describe('NativeWatcherRegistry', function () {
  let createNative, registry

  beforeEach(function () {
    registry = new NativeWatcherRegistry((normalizedPath, options) => createNative(normalizedPath, options))
  })

  it('attaches a PathWatcher to a newly created NativeWatcher for a new directory', async function () {
    const watcher = new MockWatcher(absolute('some', 'path'))
    const NATIVE = new MockNative('created')
    createNative = () => NATIVE

    await registry.attach(watcher)

    assert.strictEqual(watcher.native, NATIVE)
  })

  it('reuses an existing NativeWatcher on the same directory', async function () {
    const EXISTING = new MockNative('existing')
    const existingPath = absolute('existing', 'path')
    let firstTime = true
    createNative = () => {
      if (firstTime) {
        firstTime = false
        return EXISTING
      }

      return new MockNative('nope')
    }
    await registry.attach(new MockWatcher(existingPath))

    const watcher = new MockWatcher(existingPath)
    await registry.attach(watcher)

    assert.strictEqual(watcher.native, EXISTING)
  })

  it('is case sensitive', async function () {
    const LOWER = new MockNative('lower')
    const UPPER = new MockNative('upper')

    const lowerPath = absolute('existing', 'path')
    const upperPath = absolute('EXISTING', 'PATH')

    createNative = dir => {
      if (dir === lowerPath) return LOWER
      if (dir === upperPath) return UPPER
      return new MockNative('nope')
    }

    const lw = new MockWatcher(lowerPath)
    await registry.attach(lw)

    const uw = new MockWatcher(upperPath)
    await registry.attach(uw)

    assert.strictEqual(lw.native, LOWER)
    assert.strictEqual(uw.native, UPPER)
  })

  it('attaches to an existing NativeWatcher on a parent directory', async function () {
    const EXISTING = new MockNative('existing')
    const parentDir = absolute('existing', 'path')
    const subDir = path.join(parentDir, 'sub', 'directory')
    let firstTime = true
    createNative = () => {
      if (firstTime) {
        firstTime = false
        return EXISTING
      }

      return new MockNative('nope')
    }
    await registry.attach(new MockWatcher(parentDir))

    const watcher = new MockWatcher(subDir)
    await registry.attach(watcher)

    assert.strictEqual(watcher.native, EXISTING)
  })

  it('adopts PathWatchers from NativeWatchers on child directories', async function () {
    const parentDir = absolute('existing', 'path')
    const childDir0 = path.join(parentDir, 'child', 'directory', 'zero')
    const childDir1 = path.join(parentDir, 'child', 'directory', 'one')
    const otherDir = absolute('another', 'path')

    const CHILD0 = new MockNative('existing0')
    const CHILD1 = new MockNative('existing1')
    const OTHER = new MockNative('existing2')
    const PARENT = new MockNative('parent')

    createNative = dir => {
      if (dir === childDir0) {
        return CHILD0
      } else if (dir === childDir1) {
        return CHILD1
      } else if (dir === otherDir) {
        return OTHER
      } else if (dir === parentDir) {
        return PARENT
      } else {
        throw new Error(`Unexpected path: ${dir}`)
      }
    }

    const watcher0 = new MockWatcher(childDir0)
    await registry.attach(watcher0)

    const watcher1 = new MockWatcher(childDir1)
    await registry.attach(watcher1)

    const watcher2 = new MockWatcher(otherDir)
    await registry.attach(watcher2)

    assert.strictEqual(watcher0.native, CHILD0)
    assert.strictEqual(watcher1.native, CHILD1)
    assert.strictEqual(watcher2.native, OTHER)

    // Consolidate all three watchers beneath the same native watcher on the parent directory
    const watcher = new MockWatcher(parentDir)
    await registry.attach(watcher)

    assert.strictEqual(watcher.native, PARENT)

    assert.strictEqual(watcher0.native, PARENT)
    assert.strictEqual(CHILD0.stopped, true)

    assert.strictEqual(watcher1.native, PARENT)
    assert.strictEqual(CHILD1.stopped, true)

    assert.strictEqual(watcher2.native, OTHER)
    assert.strictEqual(OTHER.stopped, false)
  })

  describe('removing NativeWatchers', function () {
    it('happens when they stop', async function () {
      const STOPPED = new MockNative('stopped')
      const RUNNING = new MockNative('running')

      const stoppedPath = absolute('watcher', 'that', 'will', 'be', 'stopped')
      const stoppedPathParts = stoppedPath.split(path.sep).filter(part => part.length > 0)
      const runningPath = absolute('watcher', 'that', 'will', 'continue', 'to', 'exist')
      const runningPathParts = runningPath.split(path.sep).filter(part => part.length > 0)

      createNative = dir => {
        if (dir === stoppedPath) {
          return STOPPED
        } else if (dir === runningPath) {
          return RUNNING
        } else {
          throw new Error(`Unexpected path: ${dir}`)
        }
      }

      const stoppedWatcher = new MockWatcher(stoppedPath)
      await registry.attach(stoppedWatcher)

      const runningWatcher = new MockWatcher(runningPath)
      await registry.attach(runningWatcher)

      STOPPED.stop()

      const runningNode = registry.tree.root.lookup(runningPathParts).when({
        parent: node => node,
        missing: () => false,
        children: () => false
      })
      assert.isOk(runningNode)
      assert.strictEqual(runningNode.getNativeWatcher(), RUNNING)

      const stoppedNode = registry.tree.root.lookup(stoppedPathParts).when({
        parent: () => false,
        missing: () => true,
        children: () => false
      })
      assert.isTrue(stoppedNode)
    })

    it('reassigns new child watchers when a parent watcher is stopped', async function () {
      const CHILD0 = new MockNative('child0')
      const CHILD1 = new MockNative('child1')
      const PARENT = new MockNative('parent')

      const parentDir = absolute('parent')
      const childDir0 = path.join(parentDir, 'child0')
      const childDir1 = path.join(parentDir, 'child1')

      createNative = dir => {
        if (dir === parentDir) {
          return PARENT
        } else if (dir === childDir0) {
          return CHILD0
        } else if (dir === childDir1) {
          return CHILD1
        } else {
          throw new Error(`Unexpected directory ${dir}`)
        }
      }

      const parentWatcher = new MockWatcher(parentDir)
      const childWatcher0 = new MockWatcher(childDir0)
      const childWatcher1 = new MockWatcher(childDir1)

      await registry.attach(parentWatcher)
      await Promise.all([
        registry.attach(childWatcher0),
        registry.attach(childWatcher1)
      ])

      // All three watchers should share the parent watcher's native watcher.
      assert.strictEqual(parentWatcher.native, PARENT)
      assert.strictEqual(childWatcher0.native, PARENT)
      assert.strictEqual(childWatcher1.native, PARENT)

      // Stopping the parent should detach and recreate the child watchers.
      PARENT.stop()

      assert.strictEqual(childWatcher0.native, CHILD0)
      assert.strictEqual(childWatcher1.native, CHILD1)

      assert.isTrue(registry.tree.root.lookup(parts(parentDir)).when({
        parent: () => false,
        missing: () => false,
        children: () => true
      }))

      assert.isTrue(registry.tree.root.lookup(parts(childDir0)).when({
        parent: () => true,
        missing: () => false,
        children: () => false
      }))

      assert.isTrue(registry.tree.root.lookup(parts(childDir1)).when({
        parent: () => true,
        missing: () => false,
        children: () => false
      }))
    })

    it('consolidates children when splitting a parent watcher', async function () {
      const CHILD0 = new MockNative('child0')
      const PARENT = new MockNative('parent')

      const parentDir = absolute('parent')
      const childDir0 = path.join(parentDir, 'child0')
      const childDir1 = path.join(parentDir, 'child0', 'child1')

      createNative = dir => {
        if (dir === parentDir) {
          return PARENT
        } else if (dir === childDir0) {
          return CHILD0
        } else {
          throw new Error(`Unexpected directory ${dir}`)
        }
      }

      const parentWatcher = new MockWatcher(parentDir)
      const childWatcher0 = new MockWatcher(childDir0)
      const childWatcher1 = new MockWatcher(childDir1)

      await registry.attach(parentWatcher)
      await Promise.all([
        registry.attach(childWatcher0),
        registry.attach(childWatcher1)
      ])

      // All three watchers should share the parent watcher's native watcher.
      assert.strictEqual(parentWatcher.native, PARENT)
      assert.strictEqual(childWatcher0.native, PARENT)
      assert.strictEqual(childWatcher1.native, PARENT)

      // Stopping the parent should detach and create the child watchers. Both child watchers should
      // share the same native watcher.
      PARENT.stop()

      assert.strictEqual(childWatcher0.native, CHILD0)
      assert.strictEqual(childWatcher1.native, CHILD0)

      assert.isTrue(registry.tree.root.lookup(parts(parentDir)).when({
        parent: () => false,
        missing: () => false,
        children: () => true
      }))

      assert.isTrue(registry.tree.root.lookup(parts(childDir0)).when({
        parent: () => true,
        missing: () => false,
        children: () => false
      }))

      assert.isTrue(registry.tree.root.lookup(parts(childDir1)).when({
        parent: () => true,
        missing: () => false,
        children: () => false
      }))
    })

    it('splits to non-recursive and recursive NativeWatchers', async function () {
      const PARENT = new MockNative('parent')
      const CHILD0 = new MockNative('child 0')
      const CHILD1 = new MockNative('child 1')
      const CHILD2 = new MockNative('child 2')

      const parentDir = absolute('root')
      const child0Dir = absolute('root', 'nonrec0')
      const child1Dir = absolute('root', 'nonrec0', 'subdir', 'rec0')
      const child2Dir = absolute('root', 'rec1')
      const child3Dir = absolute('root', 'rec1', 'nonrec1')

      createNative = (thePath, opts) => {
        if (thePath === parentDir) {
          assert.isTrue(opts.recursive)
          return PARENT
        } else if (thePath === child0Dir) {
          assert.isFalse(opts.recursive)
          return CHILD0
        } else if (thePath === child1Dir) {
          assert.isTrue(opts.recursive)
          return CHILD1
        } else if (thePath === child2Dir) {
          assert.isTrue(opts.recursive)
          return CHILD2
        }

        return new MockNative('nope')
      }

      const parentWatcher = new MockWatcher(parentDir, { recursive: true })
      const child0Watcher = new MockWatcher(child0Dir, { recursive: false })
      const child1Watcher = new MockWatcher(child1Dir, { recursive: true })
      const child2Watcher = new MockWatcher(child2Dir, { recursive: true })
      const child3Watcher = new MockWatcher(child3Dir, { recursive: false })
      await registry.attach(parentWatcher)
      await Promise.all(
        [child0Watcher, child1Watcher, child2Watcher, child3Watcher].map(w => registry.attach(w))
      )

      assert.strictEqual(parentWatcher.native, PARENT)
      assert.strictEqual(child0Watcher.native, PARENT)
      assert.strictEqual(child1Watcher.native, PARENT)
      assert.strictEqual(child2Watcher.native, PARENT)
      assert.strictEqual(child3Watcher.native, PARENT)

      PARENT.stop()

      assert.strictEqual(child0Watcher.native, CHILD0)
      assert.strictEqual(child1Watcher.native, CHILD1)
      assert.strictEqual(child2Watcher.native, CHILD2)
      assert.strictEqual(child3Watcher.native, CHILD2)
    })
  })

  describe('non-recursive PathWatchers', function () {
    it('attach to an existing non-recursive NativeWatcher on the same directory', async function () {
      const EXISTING = new MockNative('existing')
      const existingPath = absolute('existing', 'path')
      let firstTime = true
      createNative = () => {
        if (firstTime) {
          firstTime = false
          return EXISTING
        }

        return new MockNative('nope')
      }
      await registry.attach(new MockWatcher(existingPath, { recursive: false }))

      const watcher = new MockWatcher(existingPath, { recursive: false })
      await registry.attach(watcher)

      assert.strictEqual(watcher.native, EXISTING)
    })

    describe('attach to an existing recursive NativeWatcher', function () {
      let existingPath, EXISTING

      beforeEach(async function () {
        EXISTING = new MockNative('existing')
        existingPath = absolute('existing', 'path')
        let firstTime = true
        createNative = () => {
          if (firstTime) {
            firstTime = false
            return EXISTING
          }

          return new MockNative('nope')
        }

        await registry.attach(new MockWatcher(existingPath, { recursive: true }))
      })

      it('on the same directory', async function () {
        const watcher = new MockWatcher(existingPath, { recursive: false })
        await registry.attach(watcher)
        assert.strictEqual(watcher.native, EXISTING)
      })

      it('on a parent directory', async function () {
        const childPath = absolute('existing', 'path', 'subdir')
        const watcher = new MockWatcher(childPath, { recursive: false })
        await registry.attach(watcher)
        assert.strictEqual(watcher.native, EXISTING)
      })
    })

    describe('re-attach to a new recursive NativeWatcher', function () {
      let existingPath, existingWatcher, EXISTING, CREATED

      beforeEach(async function () {
        EXISTING = new MockNative('existing')
        CREATED = new MockNative('created')
        existingPath = absolute('existing', 'path', 'subdirectory')
        let count = 0
        createNative = () => {
          if (count === 0) {
            count++
            return EXISTING
          } else if (count === 1) {
            count++
            return CREATED
          }

          return new MockNative('nope')
        }

        existingWatcher = new MockWatcher(existingPath, { recursive: false })
        await registry.attach(existingWatcher)
      })

      it('when the same directory is watched recursively', async function () {
        const watcher = new MockWatcher(existingPath, { recursive: true })
        await registry.attach(watcher)

        assert.strictEqual(watcher.native, CREATED)
        assert.strictEqual(existingWatcher.native, CREATED)
      })

      it('when a parent directory is watched recursively', async function () {
        const parentDir = absolute('existing', 'path')
        const watcher = new MockWatcher(parentDir, { recursive: true })
        await registry.attach(watcher)

        assert.strictEqual(watcher.native, CREATED)
        assert.strictEqual(existingWatcher.native, CREATED)
      })
    })

    it('does not prevent a child directory from being watched with a new recursive NativeWatcher', async function () {
      const parentDir = absolute('existing', 'path')
      const childDir = absolute('existing', 'path', 'child', 'directory')

      const PARENT = new MockNative('existing')
      const CHILD = new MockNative('created')

      createNative = (thePath, opts) => {
        if (thePath === parentDir) {
          assert.isFalse(opts.recursive)
          return PARENT
        } else if (thePath === childDir) {
          assert.isTrue(opts.recursive)
          return CHILD
        }

        return new MockNative('nope')
      }

      const parentWatcher = new MockWatcher(parentDir, { recursive: false })
      await registry.attach(parentWatcher)
      assert.strictEqual(parentWatcher.native, PARENT)

      const childWatcher = new MockWatcher(childDir, { recursive: true })
      await registry.attach(childWatcher)
      assert.strictEqual(childWatcher.native, CHILD)
      assert.strictEqual(parentWatcher.native, PARENT)
    })

    it('does not adopt PathWatchers on child directories', async function () {
      const parentDir = absolute('existing', 'path')
      const recursiveChild = absolute('existing', 'path', 'recursive')
      const nonrecursiveChild = absolute('existing', 'path', 'nonrecursive')

      const PARENT = new MockNative('parent')
      const RCHILD = new MockNative('recursive child')
      const NRCHILD = new MockNative('non-recursive child')

      createNative = (thePath, opts) => {
        if (thePath === recursiveChild && opts.recursive) {
          return RCHILD
        } else if (thePath === nonrecursiveChild && !opts.recursive) {
          return NRCHILD
        } else if (thePath === parentDir && !opts.recursive) {
          return PARENT
        }

        return new MockNative('oops')
      }

      const rChildWatcher = new MockWatcher(recursiveChild, { recursive: true })
      await registry.attach(rChildWatcher)

      const nrChildWatcher = new MockWatcher(nonrecursiveChild, { recursive: false })
      await registry.attach(nrChildWatcher)

      assert.strictEqual(rChildWatcher.native, RCHILD)
      assert.strictEqual(nrChildWatcher.native, NRCHILD)

      const parentWatcher = new MockWatcher(parentDir, { recursive: false })
      await registry.attach(parentWatcher)

      assert.strictEqual(parentWatcher.native, PARENT)
      assert.strictEqual(rChildWatcher.native, RCHILD)
      assert.strictEqual(nrChildWatcher.native, NRCHILD)

      for (const native of [PARENT, RCHILD, NRCHILD]) {
        assert.isFalse(native.stopped, `Native watcher ${native.name} was stopped`)
      }
    })
  })
})
