const { DirectoryNode } = require('./directory-node')
const { ParentResult } = require('./result')

// Private: leaf or body node within a {Tree}. Represents a directory that is watched by a non-recursive
// {NativeWatcher}.
class NonrecursiveWatcherNode extends DirectoryNode {
  // Private: Allocate a new node to track a non-recursive {NativeWatcher}.
  //
  // * `nativeWatcher` An existing {NativeWatcher} instance.
  // * `absolutePathSegments` The absolute path to this {NativeWatcher}'s directory as an {Array} of path segments.
  // * `children` {Object} mapping directory entries to immediate child nodes within the {Tree}.
  constructor (nativeWatcher, absolutePathSegments, children) {
    super(children)
    this.absolutePathSegments = absolutePathSegments
    this.nativeWatcher = nativeWatcher
  }

  // Private: Reconstruct the {NativeWatcher} options used to create our watcher.
  //
  // Returns an {Object} containing settings that will replicate the {NativeWatcher} we own.
  getOptions () {
    return { recursive: false }
  }

  // Private: Determine if this node's {NativeWatcher} will deliver at least the events requested by an options
  // {Object}.
  isCompatible (options) {
    return options.recursive === false
  }

  // Private: Ensure that only compatible, non-recursive watchers are attached here.
  addChildPath (childPathSegments, options) {
    if (!this.isCompatible(options)) {
      throw new Error(`Attempt to add an incompatible child watcher to ${this}`)
    }

    if (childPathSegments.length !== 0) {
      throw new Error(`Attempt to adopt a child watcher on ${this}`)
    }
  }

  // Private: Ensure that only exactly matching watchers have been attached here.
  removeChildPath (childPathSegments) {
    if (childPathSegments.length !== 0) {
      throw new Error(`Attempt to remove a child watcher on ${this}`)
    }
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
  // * `pathSegments` filesystem path of a new {PathWatcher} already split into an {Array} of directory names.
  //
  // Returns: A {ParentResult} referencing this node if it is an exact match, a {ParentResult} referencing an
  // descendent node if it is an exact match of the query or a parent of the query, a {ChildrenResult} if one or
  // more child paths of the request are being watched, or a {MissingResult} if no relevant watchers exist.
  lookup (pathSegments) {
    if (pathSegments.length === 0) {
      return new ParentResult(this, pathSegments)
    }

    return super.lookup(pathSegments)
  }

  // Private: Become a regular {DirectoryNode} if the watcher's exact path matches.
  //
  // * `pathSegments` absolute pre-split filesystem path of the node to remove.
  // * `createSplitNative` callback to be invoked with each child path segment {Array} if the {RecursiveWatcherNode}
  //   is split into child watchers rather than removed outright. See {RecursiveWatcherNode.remove}. If `null`,
  //   no child node splitting will occur.
  //
  // Returns: The root of a new tree with the node removed. Callers should replace their node references with the
  // returned value.
  remove (pathSegments, createSplitNative) {
    if (pathSegments.length === 0 && Object.keys(this.children).length > 0) {
      // Become a regular DirectoryNode with the same children.
      return new DirectoryNode(this.children)
    }

    return super.remove(pathSegments, createSplitNative)
  }

  // Private: Discover all node instances beneath this tree node associated with a {NativeWatcher} and the child paths
  // that they are watching.
  //
  // * `prefix` {Array} of intermediate path segments to prepend to the resulting child paths.
  //
  // Returns: A possibly empty {Array} of `{node, path}` objects describing {RecursiveWatcherNode} and
  // {NonrecursiveWatcherNode} instances beneath this node, including this node.
  childWatchers (prefix) {
    return [{ node: this, path: prefix }, ...super.childWatchers(prefix)]
  }

  // Private: Return a {String} representation of this watcher and its descendents for diagnostics and testing.
  print (indent = 0) {
    let result = ''
    for (let i = 0; i < indent; i++) {
      result += ' '
    }
    result += '[non-recursive watcher]\n'
    return result + super.print(indent)
  }
}

module.exports = { NonrecursiveWatcherNode }
