// Use the compilation database to run include-what-you-use on each source file.

const path = require('path')
const compileCommands = require('../../build/compile_commands')
const {spawn} = require('child_process')

const BUILD_DIR = path.resolve(__dirname, '..', '..', 'build')

function runIwyu (compileEntry) {
  const clangOpts = compileEntry.command.replace(/^c\+\+ /, '')
  console.log(clangOpts)

  return new Promise(resolve => {
    const iwyu = spawn(`iwyu ${clangOpts}`, {
      cwd: BUILD_DIR,
      shell: true,
      stdio: 'inherit'
    })

    iwyu.on('close', code => {
      if (code !== 0) {
        console.log(`exited with status ${code}`)
      }
      resolve()
    })
  })
}

async function runIwyuAll () {
  for (const entry of compileCommands) {
    await runIwyu(entry)
  }
}

runIwyuAll().then(
  () => process.exit(0),
  err => {
    console.error(err)
    process.exit(1)
  }
)
