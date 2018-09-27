const { MissingResult, ChildrenResult } = require('./result')

// Private: Non-leaf node in a {Tree} used by the {NativeWatcherRegistry} to cover the allocated {PathWatcher}
// instances with the most efficient set of {NativeWatcher} instances possible. Each {DirectoryNode} maps to a directory
// in the filesystem tree.
class DirectoryNode {
  // Private: Construct a new, empty node representing a node with no watchers.
  constructor (children) {
    this.children = children || {}
  }

  // Private: Recursively discover any existing watchers corresponding to a path.
  //
  // * `pathSegments` filesystem path of a new {PathWatcher} already split into an Array of directory names.
  //
  // Returns: A {ParentResult} if the exact requested directory or a parent directory is being watched, a
  //   {ChildrenResult} if one or more child paths are being watched, or a {MissingResult} if no relevant watchers
  //   exist.
  lookup (pathSegments) {
    if (pathSegments.length === 0) {
      return new ChildrenResult(this.childWatchers([]), this.children)
    }

    const child = this.children[pathSegments[0]]
    if (child === undefined) {
      return new MissingResult(this)
    }

    return child.lookup(pathSegments.slice(1))
  }

  // Private: Insert a new watcher node into the tree, creating new intermediate {DirectoryNode} instances as
  // needed. Existing children of the watched directory are adopted as children by a new {NonrecursiveWatcherNode}
  // or removed by a new {RecursiveWatcherNode}.
  //
  // * `pathSegments` filesystem path of the new {PathWatcher}, already split into an Array of directory names.
  // * `node` initialized {RecursiveWatcherNode} or {NonrecursiveWatcherNode} to insert.
  //
  // Returns: The root of a new tree with the new node inserted at the correct location. Callers should replace their
  // node references with the returned value.
  insert (pathSegments, node) {
    if (pathSegments.length === 0) {
      return node
    }

    const pathKey = pathSegments[0]
    let child = this.children[pathKey]
    if (child === undefined) {
      child = new DirectoryNode()
    }
    this.children[pathKey] = child.insert(pathSegments.slice(1), node)
    return this
  }

  // Private: Remove a {RecursiveWatcherNode} by its exact watched directory.
  //
  // * `pathSegments` absolute pre-split filesystem path of the node to remove.
  // * `createSplitNative` callback to be invoked with each child path segment {Array} if the {RecursiveWatcherNode}
  //   is split into child watchers rather than removed outright. See {RecursiveWatcherNode.remove}. If `null`,
  //   no child node splitting will occur.
  //
  // Returns: The root of a new tree with the node removed. Callers should replace their node references with the
  // returned value.
  remove (pathSegments, createSplitNative) {
    if (pathSegments.length === 0) {
      // Attempt to remove a path with child watchers. Do nothing.
      return this
    }

    const pathKey = pathSegments[0]
    const child = this.children[pathKey]
    if (child === undefined) {
      // Attempt to remove a path that isn't watched. Do nothing.
      return this
    }

    // Recurse
    const newChild = child.remove(pathSegments.slice(1), createSplitNative)
    if (newChild === null) {
      delete this.children[pathKey]
    } else {
      this.children[pathKey] = newChild
    }

    // Remove this node if all of its children have been removed
    return Object.keys(this.children).length === 0 ? null : this
  }

  // Private: Discover all node instances beneath this tree node associated with a {NativeWatcher} and the child paths
  // that they are watching.
  //
  // * `prefix` {Array} of intermediate path segments to prepend to the resulting child paths.
  //
  // Returns: A possibly empty {Array} of `{node, path}` objects describing {RecursiveWatcherNode} and
  // {NonrecursiveWatcherNode} instances beneath this node.
  childWatchers (prefix) {
    const results = []
    for (const p of Object.keys(this.children)) {
      results.push(...this.children[p].childWatchers(prefix.concat([p])))
    }
    return results
  }

  // Private: Return a {String} representation of this subtree for diagnostics and testing.
  print (indent = 0) {
    let spaces = ''
    for (let i = 0; i < indent; i++) {
      spaces += ' '
    }

    let result = ''
    for (const p of Object.keys(this.children)) {
      result += `${spaces}${p}\n${this.children[p].print(indent + 2)}`
    }
    return result
  }
}

module.exports = { DirectoryNode }
