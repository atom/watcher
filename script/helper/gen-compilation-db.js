// Generate an LLVM compilation database from verbose Make output. This is a JSON file used by LLVM tools to
// consistently operate on the files in a build tree. See:
//  https://clang.llvm.org/docs/JSONCompilationDatabase.html
// for a description of the format.

const path = require('path')
const fs = require('fs-extra')
const readline = require('readline')
const {spawn} = require('child_process')
const shell = require('shell-quote')

const BUILD_DIR = path.resolve(__dirname, '..', '..', 'build')
const OUTPUT_FILE = path.join(BUILD_DIR, 'compile_commands.json')

class CompilationDatabase {
  constructor () {
    this.entries = []
  }

  addEntryForCompilation (line) {
    const sourceFiles = shell.parse(line).filter(word => word.endsWith('.cpp'))
    if (sourceFiles.length === 0) {
      console.error(`No source file parsed from: ${line}`)
      return
    }
    if (sourceFiles.length > 1) {
      console.error(`Ambiguous source files parsed from: ${line}`)
      return
    }
    const sourceFile = sourceFiles[0]

    this.entries.push({
      directory: BUILD_DIR,
      command: line.trim(),
      file: path.resolve(BUILD_DIR, sourceFile)
    })
  }

  write () {
    return fs.writeFile(OUTPUT_FILE, JSON.stringify(this.entries, null, '  '))
  }
}

async function runNodeGyp () {
  const db = new CompilationDatabase()

  return new Promise((resolve, reject) => {
    const nodeGyp = spawn('node-gyp', process.argv.slice(2), {
      env: Object.assign({}, process.env, {V: '1'}),
      stdio: [process.stdin, 'pipe', process.stderr]
    })
    const lineReader = readline.createInterface({input: nodeGyp.stdout})

    lineReader.on('line', line => {
      if (/-DNODE_GYP_MODULE_NAME=/.test(line)) {
        db.addEntryForCompilation(line)
        process.stdout.write(`BUILD COMMAND:${line}\n`)
      } else {
        process.stdout.write(`${line}\n`)
      }
    })

    nodeGyp.on('close', code => {
      if (code === 0) {
        resolve(db.write())
      } else {
        const e = new Error('node-gyp failure')
        e.code = code
        reject(e)
      }
    })
  })
}

runNodeGyp().then(
  () => process.exit(0),
  err => {
    console.error(err)
    process.exit(err.code || 1)
  }
)
