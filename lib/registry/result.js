// Private: A {DirectoryNode} traversal result that's returned when neither a directory, its children, nor its parents
// are present in the tree.
class MissingResult {
  // Private: Instantiate a new {MissingResult}.
  //
  // * `lastParent` the final successfully traversed {DirectoryNode}.
  constructor (lastParent) {
    this.lastParent = lastParent
  }

  // Private: Dispatch within a map of callback actions.
  //
  // * `actions` {Object} containing a `missing` key that maps to a callback to be invoked when no results were returned
  //   by {DirectoryNode.lookup}. The callback will be called with the last parent node that was encountered during the
  //   traversal.
  //
  // Returns: the result of the `actions` callback.
  when (actions) {
    return actions.missing(this.lastParent)
  }
}

// Private: A {DirectoryNode.lookup} traversal result that's returned when a parent or an exact match of the requested
// directory is being watched by an existing {RecursiveWatcherNode}.
class ParentResult {
  // Private: Instantiate a new {ParentResult}.
  //
  // * `parent` the {RecursiveWatcherNode} that was discovered.
  // * `remainingPathSegments` an {Array} of the directories that lie between the leaf node's watched directory and
  //   the requested directory. This will be empty for exact matches.
  constructor (parent, remainingPathSegments) {
    this.parent = parent
    this.remainingPathSegments = remainingPathSegments
  }

  // Private: Dispatch within a map of callback actions.
  //
  // * `actions` {Object} containing a `parent` key that maps to a callback to be invoked when a parent of a requested
  //   requested directory is returned by a {DirectoryNode.lookup} call. The callback will be called with the
  //   {RecursiveWatcherNode} instance and an {Array} of the {String} path segments that separate the parent node
  //   and the requested directory.
  //
  // Returns: the result of the `actions` callback.
  when (actions) {
    return actions.parent(this.parent, this.remainingPathSegments)
  }
}

// Private: A {DirectoryNode.lookup} traversal result that's returned when one or more children of the requested
// directory are already being watched.
class ChildrenResult {
  // Private: Instantiate a new {ChildrenResult}.
  //
  // * `children` {Array} containing objects with:
  //   * `node` {RecursiveWatcherNode} or {NonrecursiveWatcherNode} instance that was discovered.
  //   * `path` the relative path between the query and the corresponding node.
  // * `immediate` {Object} containing the child node map from the last node traversed.
  constructor (children, immediate) {
    this.children = children
    this.immediate = immediate
  }

  // Private: Dispatch within a map of callback actions.
  //
  // * `actions` {Object} containing a `children` key that maps to a callback to be invoked when a parent of a requested
  //   requested directory is returned by a {DirectoryNode.lookup} call. The callback will be called with the
  //   {RecursiveWatcherNode} instance.
  //
  // Returns: the result of the `actions` callback.
  when (actions) {
    return actions.children(this.children, this.immediate)
  }
}

module.exports = { MissingResult, ParentResult, ChildrenResult }
