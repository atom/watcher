const {DirectoryNode} = require('./directory-node')
const {ParentResult} = require('./result')

class NonrecursiveWatcherNode extends DirectoryNode {
  constructor (nativeWatcher, absolutePathSegments, children) {
    super(children)
    this.absolutePathSegments = absolutePathSegments
    this.nativeWatcher = nativeWatcher
  }

  getOptions () {
    return {recursive: false}
  }

  isCompatible (options) {
    return options.recursive === false
  }

  getAbsolutePathSegments () {
    return this.absolutePathSegments
  }

  addChildPath (childPathSegments, options) {
    if (!this.isCompatible(options)) {
      throw new Error(`Attempt to add an incompatible child watcher to ${this}`)
    }

    if (childPathSegments.length !== 0) {
      throw new Error(`Attempt to adopt a child watcher on ${this}`)
    }
  }

  removeChildPath (childPathSegments) {
    if (childPathSegments.length !== 0) {
      throw new Error(`Attempt to remove a child watcher on ${this}`)
    }
  }

  lookup (pathSegments) {
    if (pathSegments.length === 0) {
      return new ParentResult(this, pathSegments)
    }

    return super.lookup(pathSegments)
  }

  remove (pathSegments, createSplitNative) {
    if (pathSegments.length === 0 && Object.keys(this.children).length > 0) {
      // Become a regular DirectoryNode with the same children.
      return new DirectoryNode(this.children)
    }

    return super.remove(pathSegments, createSplitNative)
  }

  childWatchers (prefix) {
    return [{node: this, path: prefix}, ...super.childWatchers(prefix)]
  }

  getNativeWatcher () {
    return this.nativeWatcher
  }

  print (indent = 0) {
    let result = ''
    for (let i = 0; i < indent; i++) {
      result += ' '
    }
    result += '[non-recursive watcher]\n'
    return result + super.print(indent)
  }
}

module.exports = {NonrecursiveWatcherNode}
