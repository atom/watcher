const fs = require('fs-extra')

const { Fixture } = require('../helper')
const { EventMatcher } = require('../matcher')

describe('paths with extended utf8 characters', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  it('creates watches and reports event paths', async function () {
    // Thanks, http://www.i18nguy.com/unicode/supplementary-test.html
    // I sure hope these don't mean anything really obscene!
    const rootDir = fixture.watchPath('𠜎')
    const fileName = fixture.watchPath('𠜎', '𤓓')

    await fs.mkdirs(rootDir)

    const matcher = new EventMatcher(fixture)
    await matcher.watch(['𠜎'], {})

    await fs.writeFile(fileName, 'wat\n')

    await until('creation event arrives', matcher.allEvents(
      { action: 'created', kind: 'file', path: fileName }
    ))
  })
})
