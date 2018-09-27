// Generate an LLVM compilation database from verbose Make output. This is a JSON file used by LLVM tools to
// consistently operate on the files in a build tree. See:
//  https://clang.llvm.org/docs/JSONCompilationDatabase.html
// for a description of the format.

const path = require('path')
const readline = require('readline')
const { spawn, execFileSync } = require('child_process')

const BASE_DIR = path.resolve(__dirname, '..', '..')
const BUILD_DIR = path.resolve(BASE_DIR, 'build')
const OUTPUT_FILE = path.join(BUILD_DIR, 'compile_commands.json')
const NODE_GYP_BINARY = process.platform === 'win32' ? 'node-gyp.cmd' : 'node-gyp'
const VERBOSE = process.env.V === '1'

let fs, shell
try {
  fs = require('fs-extra')
  shell = require('shell-quote')
} catch (e) {
  execFileSync(NODE_GYP_BINARY, process.argv.slice(2), { stdio: 'inherit' })
  process.exit(0)
}

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

    const entry = {
      directory: BUILD_DIR,
      command: line.trim(),
      file: path.resolve(BUILD_DIR, sourceFile)
    }
    this.entries.push(entry)
    return entry
  }

  write () {
    return fs.writeFile(OUTPUT_FILE, JSON.stringify(this.entries, null, '  '))
  }
}

function runNodeGyp () {
  const db = new CompilationDatabase()

  return new Promise((resolve, reject) => {
    const nodeGyp = spawn(NODE_GYP_BINARY, process.argv.slice(2), {
      env: Object.assign({}, process.env, { V: '1' }),
      stdio: [process.stdin, 'pipe', process.stderr]
    })
    const lineReader = readline.createInterface({ input: nodeGyp.stdout })

    lineReader.on('line', line => {
      if (/-DNODE_GYP_MODULE_NAME=/.test(line)) {
        const entry = db.addEntryForCompilation(line)
        if (VERBOSE) {
          process.stdout.write(`build command [${line}]\n`)
        } else {
          const relPath = path.relative(BASE_DIR, entry.file)
          process.stdout.write(` building ${relPath}\n`)
        }
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
