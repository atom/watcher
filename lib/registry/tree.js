const { absolute } = require('./helper')

const { DirectoryNode } = require('./directory-node')
const { RecursiveWatcherNode } = require('./recursive-watcher-node')
const { NonrecursiveWatcherNode } = require('./nonrecursive-watcher-node')
const { log } = require('../logger')

// Private: Map userland filesystem watcher subscriptions efficiently to deliver filesystem change notifications to
// each watcher with the most efficient coverage of native watchers.
//
// * If two watchers subscribe to the same directory, use a single native watcher for each.
// * Re-use a native watcher watching a parent directory for a watcher on a child directory. If the parent directory
//   watcher is removed, it will be split into child watchers.
// * If any child directories already being watched, stop and replace them with a watcher on the parent directory.
//
// Uses a trie whose structure mirrors the directory structure.
class Tree {
  // Private: Construct a tree with no native watchers.
  //
  // * `basePathSegments` the position of this tree's root relative to the filesystem's root as an {Array} of directory
  //   names.
  // * `createNative` {Function} used to construct new native watchers. It should accept an absolute path and an options
  //   {Object} as arguments and return a new {NativeWatcher}.
  constructor (basePathSegments, createNative) {
    this.basePathSegments = basePathSegments
    this.root = new DirectoryNode()
    this.createNative = createNative
  }

  // Private: Identify the native watcher that should be used to produce events at a watched path, creating a new one
  // if necessary.
  //
  // * `pathSegments` the path to watch represented as an {Array} of directory names relative to this {Tree}'s
  //   root.
  // * `options` {Object} Options used to create a new {NativeWatcher}. This may also affect the kind of node inserted
  //   into the {Tree} and the way that existing and future nodes will be consolidated with it.
  // * `attachToNative` {Function} invoked with the appropriate native watcher and the absolute path to its watch root.
  add (pathSegments, options, attachToNative) {
    const absolutePathSegments = this.basePathSegments.concat(pathSegments)
    const absolutePath = absolute(...absolutePathSegments)
    log('Tree: adding %j with options %j. absolute = %s.', pathSegments, options, absolutePath)

    const attachToNew = (children, immediate) => {
      const native = this.createNative(absolutePath, options)
      const node = options.recursive
        ? new RecursiveWatcherNode(native, absolutePathSegments, children)
        : new NonrecursiveWatcherNode(native, absolutePathSegments, immediate)
      this.root = this.root.insert(pathSegments, node)

      const sub = native.onWillStop(split => {
        sub.dispose()
        const createNative = split ? this.createNative : null
        this.root = this.root.remove(pathSegments, createNative) || new DirectoryNode()
      })

      attachToNative(native, absolutePath, options)
      return native
    }

    this.root.lookup(pathSegments).when({
      parent: (parent, remaining) => {
        log('Tree: discovered parent node with remaining segments %j.', remaining)

        // An existing NativeWatcher is watching the same directory or a parent directory of the requested path.
        const existingNative = parent.getNativeWatcher()
        const absoluteParentPath = absolute(...parent.getAbsolutePathSegments())

        if (parent.isCompatible(options)) {
          log('Tree: adding child path and attaching to existing native %s.', existingNative)

          // Attach this Watcher to it as a filtering watcher and record it as a dependent child path.
          parent.addChildPath(remaining, options)
          attachToNative(existingNative, absoluteParentPath, parent.getOptions())
        } else {
          log('Tree: broadening existing native watcher %s.', existingNative)

          // Construct and attach a new {NativeWatcher} that will deliver events suitable for both the old and
          // new watchers. Reattach consumers of the existing {NativeWatcher} and stop it.
          const newNative = attachToNew([], {})
          existingNative.reattachTo(newNative, absoluteParentPath, options)
          existingNative.stop()
        }
      },
      children: (children, immediate) => {
        log('Tree: discovered watched children %j.', children)

        // One or more NativeWatchers exist on child directories of the requested path. Create a new native watcher
        // on the parent directory.
        const newNative = attachToNew(children, immediate)

        if (options.recursive) {
          log('Tree: adopting %d existing children.', children.length)
          // Create a new native watcher on the parent directory, note the subscribed child paths, and cleanly stop the
          // child native watchers.

          for (let i = 0; i < children.length; i++) {
            const childNode = children[i].node
            const childNative = childNode.getNativeWatcher()
            childNative.reattachTo(newNative, absolutePath, options)
            childNative.stop()
          }
        }
      },
      missing: () => {
        log('Tree: creating and attaching new native watcher.')
        attachToNew([], {})
      }
    })
  }

  // Private: Access the root node of the tree.
  getRoot () {
    return this.root
  }

  // Private: Return a {String} representation of this tree's structure for diagnostics and testing.
  print () {
    return this.root.print()
  }
}

module.exports = { Tree }
