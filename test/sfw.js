const sfw = require('../lib')

describe('entry point', function () {
  it('calls a C++ function correctly', function () {
    assert.equal(sfw.ok(), 'whatever')
  })
})
