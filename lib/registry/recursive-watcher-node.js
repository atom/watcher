const path = require('path')

const { ParentResult } = require('./result')
let Tree = null

// Private: Leaf node within a {NativeWatcherRegistry} tree. Represents a directory that is covered by a
// recursive {NativeWatcher}.
class RecursiveWatcherNode {
  // Private: Allocate a new node to track a {NativeWatcher}.
  //
  // * `nativeWatcher` An existing {NativeWatcher} instance.
  // * `absolutePathSegments` The absolute path to this {NativeWatcher}'s directory as an {Array} of
  //   path segments.
  // * `children` {Array} of information about child nodes that are to become the responsibility of this
  //   {NativeWatcher}. Children are represented as objects containing:
  //   * `path` Relative path to the watched child directory, represented as an {Array} of path segments between
  //     this node's directory and the watched child path.
  //   * `node` Instance of {RecursiveWatcherNode} or {NonrecursiveWatcherNode} currently responsible for this path
  //     within the {Tree}.
  constructor (nativeWatcher, absolutePathSegments, children) {
    this.nativeWatcher = nativeWatcher
    this.absolutePathSegments = absolutePathSegments

    // Store child paths as joined strings so they work as Set members.
    this.adopted = new Map()
    for (let i = 0; i < children.length; i++) {
      const childPath = path.join(...children[i].path)
      const childNode = children[i].node

      this.adopted.set(childPath, childNode.getOptions())
    }
  }

  // Private: Reconstruct the {NativeWatcher} options used to create our watcher.
  //
  // Returns an {Object} containing settings that will replicate the {NativeWatcher} we own.
  getOptions () {
    return { recursive: true }
  }

  // Private: Determine if this node's {NativeWatcher} will deliver at least the events requested by an options
  // {Object}.
  isCompatible (options) {
    return true
  }

  // Private: Assume responsibility for a new child path. If this node is removed, it will instead
  // split into a subtree with a new {RecursiveWatcherNode} for each child path.
  //
  // * `childPathSegments` the {Array} of path segments between this node's directory and the watched
  //   child directory.
  // * `options` an {Object} containing settings that configured the child watcher at this path.
  addChildPath (childPathSegments, options) {
    const childPath = path.join(...childPathSegments)
    this.adopted.set(childPath, options)
  }

  // Private: Stop assuming responsibility for a previously assigned child path. If this node is
  // removed, the named child path will no longer be allocated a {RecursiveWatcherNode}.
  //
  // * `childPathSegments` the {Array} of path segments between this node's directory and the no longer
  //   watched child directory.
  removeChildPath (childPathSegments) {
    this.adopted.delete(path.join(...childPathSegments))
  }

  // Private: Accessor for the {NativeWatcher}.
  getNativeWatcher () {
    return this.nativeWatcher
  }

  // Private: Return the absolute path watched by this {NativeWatcher} as an {Array} of directory names.
  getAbsolutePathSegments () {
    return this.absolutePathSegments
  }

  // Private: Identify how this watcher relates to a request to watch a directory tree.
  //
  // * `pathSegments` filesystem path of a new {PathWatcher} already split into an Array of directory names.
  //
  // Returns: A {ParentResult} referencing this node.
  lookup (pathSegments) {
    return new ParentResult(this, pathSegments)
  }

  // Private: Remove this leaf node if the watcher's exact path matches. If this node is covering additional
  // {PathWatcher} instances on child paths, it will be split into a subtree.
  //
  // * `pathSegments` filesystem path of the node to remove.
  // * `createSplitNative` callback invoked with each {Array} of absolute child path segments to create a native
  //   watcher on a subtree of this node. If `null`, no child splitting will occur.
  //
  // Returns: If `pathSegments` match this watcher's path exactly, returns `null` if this node has no `childPaths`
  // or a new {DirectoryNode} on a newly allocated subtree if it did. If `pathSegments` does not match the watcher's
  // path, it's an attempt to remove a subnode that doesn't exist, so the remove call has no effect and returns
  // `this` unaltered.
  remove (pathSegments, createSplitNative) {
    if (pathSegments.length !== 0) {
      return this
    } else if (this.adopted.size > 0 && createSplitNative !== null) {
      if (!Tree) {
        Tree = require('./tree').Tree
      }

      let newSubTree = new Tree(this.absolutePathSegments, createSplitNative)

      for (const [childPath, childOptions] of this.adopted) {
        const childPathSegments = childPath.split(path.sep)
        newSubTree.add(childPathSegments, childOptions, (native, attachmentPath) => {
          this.nativeWatcher.reattachTo(native, attachmentPath, childOptions)
        })
      }

      return newSubTree.getRoot()
    } else {
      return null
    }
  }

  // Private: Discover this {RecursiveWatcherNode} instance.
  //
  // * `prefix` {Array} of intermediate path segments to prepend to the resulting child paths.
  //
  // Returns: An {Array} containing a `{node, path}` object describing this node.
  childWatchers (prefix) {
    return [{ node: this, path: prefix }]
  }

  // Private: Return a {String} representation of this watcher for diagnostics and testing. Indicates the number of
  // child paths that this node's {NativeWatcher} is responsible for.
  print (indent = 0) {
    let result = ''
    for (let i = 0; i < indent; i++) {
      result += ' '
    }
    result += '[watcher'
    if (this.adopted.size > 0) {
      result += ` +${this.adopted.size}`
    }
    result += ']\n'

    return result
  }
}

module.exports = { RecursiveWatcherNode }
