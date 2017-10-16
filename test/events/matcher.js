function specMatches (spec, event) {
  return (spec.action === undefined || event.action === spec.action) &&
    (spec.kind === undefined || event.kind === spec.kind) &&
    (spec.path === undefined || event.path === spec.path) &&
    (spec.oldPath === undefined || event.oldPath === spec.oldPath)
}

class EventMatcher {
  constructor (fixture) {
    this.fixture = fixture

    this.errors = []
    this.events = []
  }

  watch (options) {
    return this.fixture.watch([], options, (err, events) => {
      this.errors.push(err)
      this.events.push(...events)
    })
  }

  allEvents (...specs) {
    const remaining = new Set(specs)

    return () => {
      for (const event of this.events) {
        for (const spec of remaining) {
          if (specMatches(spec, event)) {
            remaining.delete(spec)
          }
        }
      }

      return remaining.size === 0
    }
  }

  orderedEvents (...specs) {
    return () => {
      let specIndex = 0

      for (const event of this.events) {
        if (specs[specIndex] && specMatches(specs[specIndex], event)) {
          specIndex++
        }
      }

      return specIndex >= specs.length
    }
  }
}

module.exports = {EventMatcher}
