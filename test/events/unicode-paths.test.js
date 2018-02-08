const fs = require('fs-extra')

const {Fixture} = require('../helper')
const {EventMatcher} = require('../matcher')

describe('paths with extended utf8 characters', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  it('creates watches and reports event paths', async function () {
    // Thanks, http://www.i18nguy.com/unicode/supplementary-test.html
    // I sure hope these don't mean anything really obscene!
    const rootDir = fixture.watchPath('𠜎 𠜱 𠝹 𠱓 𠱸 ')
    const fileName = fixture.watchPath('𠜎 𠜱 𠝹 𠱓 𠱸 ', '𢵧 𢺳 𣲷 𤓓')

    await fs.mkdir(rootDir)

    const matcher = new EventMatcher(fixture)
    await matcher.watch(['𠜎 𠜱 𠝹 𠱓 𠱸 '], {})

    await fs.writeFile(fileName, 'wat\n')

    await until('creation event arrives', matcher.allEvents(
      {action: 'created', kind: 'file', path: fileName}
    ))
  })
})
