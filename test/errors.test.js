/* eslint-dev mocha */

const { Fixture } = require('./helper')
const { EventMatcher } = require('./matcher')

describe('error reporting', function () {
  let fixture, matcher

  beforeEach(async function () {
    fixture = new Fixture()

    await fixture.before()
    await fixture.log()

    matcher = new EventMatcher(fixture)
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  it('rejects the promise if the path does not exist', async function () {
    await assert.isRejected(matcher.watch(['nope'], {}))
  })
})
