// Private: A {RegistryNode} traversal result that's returned when neither a directory, its children, nor its parents
// are present in the tree.
class MissingResult {
  // Private: Instantiate a new {MissingResult}.
  //
  // * `lastParent` the final successfully traversed {RegistryNode}.
  constructor (lastParent) {
    this.lastParent = lastParent
  }

  // Private: Dispatch within a map of callback actions.
  //
  // * `actions` {Object} containing a `missing` key that maps to a callback to be invoked when no results were returned
  //   by {RegistryNode.lookup}. The callback will be called with the last parent node that was encountered during the
  //   traversal.
  //
  // Returns: the result of the `actions` callback.
  when (actions) {
    return actions.missing(this.lastParent)
  }
}

// Private: A {RegistryNode.lookup} traversal result that's returned when a parent or an exact match of the requested
// directory is being watched by an existing {RegistryWatcherNode}.
class ParentResult {
  // Private: Instantiate a new {ParentResult}.
  //
  // * `parent` the {RegistryWatcherNode} that was discovered.
  // * `remainingPathSegments` an {Array} of the directories that lie between the leaf node's watched directory and
  //   the requested directory. This will be empty for exact matches.
  constructor (parent, remainingPathSegments) {
    this.parent = parent
    this.remainingPathSegments = remainingPathSegments
  }

  // Private: Dispatch within a map of callback actions.
  //
  // * `actions` {Object} containing a `parent` key that maps to a callback to be invoked when a parent of a requested
  //   requested directory is returned by a {RegistryNode.lookup} call. The callback will be called with the
  //   {RegistryWatcherNode} instance and an {Array} of the {String} path segments that separate the parent node
  //   and the requested directory.
  //
  // Returns: the result of the `actions` callback.
  when (actions) {
    return actions.parent(this.parent, this.remainingPathSegments)
  }
}

// Private: A {RegistryNode.lookup} traversal result that's returned when one or more children of the requested
// directory are already being watched.
class ChildrenResult {
  // Private: Instantiate a new {ChildrenResult}.
  //
  // * `children` {Array} of the {RegistryWatcherNode} instances that were discovered.
  constructor (children) {
    this.children = children
  }

  // Private: Dispatch within a map of callback actions.
  //
  // * `actions` {Object} containing a `children` key that maps to a callback to be invoked when a parent of a requested
  //   requested directory is returned by a {RegistryNode.lookup} call. The callback will be called with the
  //   {RegistryWatcherNode} instance.
  //
  // Returns: the result of the `actions` callback.
  when (actions) {
    return actions.children(this.children)
  }
}

module.exports = {MissingResult, ParentResult, ChildrenResult}
