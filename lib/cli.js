#!/usr/bin/env node

const path = require('path')
const watcher = require('./index')

function usage () {
  console.log('Usage: watcher <pattern> [<pattern>...] [options]')
  console.log('  -h, --help\tShow help')
  console.log('  -v, --verbose\tMake output more verbose')
}

function start (dirs, verbose) {
  const options = { recursive: true }

  const eventCallback = events => {
    for (const event of events) {
      if (event.action === 'modified' && !verbose) {
        return
      } else if (event.action === 'renamed') {
        console.log(
          `${event.action} ${event.kind}: ${event.oldPath} → ${event.path}`
        )
      } else {
        console.log(`${event.action} ${event.kind}: ${event.path}`)
      }
    }
  }

  for (const dir of dirs) {
    watcher
      .watchPath(dir, options, eventCallback)
      .then(w => {
        if (verbose) {
          console.log('Watching', dir)
        }
        w.onDidError(err => console.error('Error:', err))
      })
      .catch(err => {
        console.error('Error:', err)
      })
  }
}

function main (argv) {
  const dirs = []
  let verbose = false

  argv.forEach((arg, i) => {
    if (i === 0) {
      return
    }
    if (i === 1 && path.basename(argv[0]) === 'node') {
      return
    }
    if (arg === '-h' || arg === '--help') {
      return usage()
    } else if (arg === '-v' || arg === '--verbose') {
      verbose = true
    } else {
      dirs.push(arg)
    }
  })

  if (dirs.length === 0) {
    return usage()
  }

  start(dirs, verbose)
}

main(process.argv)
