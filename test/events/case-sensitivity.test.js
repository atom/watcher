const fs = require('fs-extra')
const path = require('path')

const {Fixture} = require('../helper')

// Probe the current filesystem to see its case sensitivity properties.
const probeDir = path.join(__dirname, 'fixture', 'probe')
let hasMixed = false
let hasLower = false
try {
  const mixedFile = path.join(probeDir, 'fileNAME.txt')
  const lowerFile = path.join(probeDir, 'filename.txt')

  fs.mkdirsSync(probeDir)

  fs.writeFileSync(mixedFile, '\n')
  fs.writeFileSync(lowerFile, '\n')

  const entries = fs.readdirSync(probeDir)
  hasMixed = entries.includes(mixedFile)
  hasLower = entries.includes(lowerFile)
} finally {
  fs.removeSync(probeDir)
}

const caseSensitive = hasMixed && hasLower
const caseInsensitive = !caseSensitive

describe('filesystem attributes', function () {
  let fixture

  beforeEach(async function () {
    fixture = new Fixture()
    await fixture.before()
    await fixture.log()
  })

  afterEach(async function () {
    await fixture.after(this.currentTest)
  })

  if (caseInsensitive) {
    describe('on a case-insensitive filesystem', function () {
      it('consolidates watch roots created with different cases', async function () {
        await fs.mkdirs(fixture.watchPath('saME'))

        const [uw, lw] = await Promise.all([
          fixture.watch(['SAME'], {}),
          fixture.watch(['same'], {})
        ])

        assert.strictEqual(uw.native, lw.native)
      })
    })
  }

  if (caseSensitive) {
    describe('on a case-sensitive filesystem', function () {
      it('distinguishes multiple watch roots with different cases', async function () {
        const upper = fixture.watchPath('DIFFERENT')
        const lower = fixture.watchPath('different')

        await Promise.all([
          fs.mkdirs(upper),
          fs.mkdirs(lower)
        ])

        const [uw, lw] = await Promise.all([
          fixture.watch(['DIFFERENT'], {}),
          fixture.watch(['different'], {})
        ])

        assert.notStrictEqual(uw.native, lw.native)
      })
    })
  }
})
