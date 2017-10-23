# So you want to help out

First of all: Thank you! Shipping complicated native code into Atom is risky, and every bug we find now saves scores of Atom users from unceremonious crashes or lock-ups. I'm going to be working on some more intensive randomized testing soon, but having multiple pairs of eyes on this is critical to avoid the pitfalls of "works on my machine" syndrome.

I've written a quick Swiss-army-knife utility to exercise @atom/watcher and compare it to other watching libraries at [@smashwilson/watcher-stress](https://github.com/smashwilson/watcher-stress). You can clone it and run it from that repository:

```bash
$ git clone git@github.com:smashwilson/watcher-stress.git
$ cd watcher-stress
$ npm_config_debug="true" npm install

# ...

$ script/watcher-stress --help
```

Or, you can install it directly from npm:

```bash
$ npm_config_debug="true" npm install -g @smashwilson/watcher-stress

# ...

$ watcher-stress --help
```

Or even run it directly from `npx`, although that will recompile all of its native dependencies on each invocation:

```bash
$ npm_config_debug="true" npx @smashwilson/watcher-stress --help

# ...
```

## Ways to help

Before you begin:

```bash
# On MacOS and Linux, be sure you're generating core files in this shell session.
$ ulimit -c unlimited

# Create a directory to write logs to *that you aren't watching.*
$ mkdir ~/watcher-logs
```

* **Leave the CLI running on a directory you're working in day to day.**

  The easiest way to help is by keeping a terminal up and running the CLI in the background while you're doing other stuff. You'll be able to see if the process crashes, churns CPU, or generates events that don't make sense.

  ```bash
  # Start the watcher CLI (takes over your terminal, Ctrl-C to stop)
  $ watcher-stress --logging-dir ~/watcher-logs --cli ~/github
  ```

* **Run the stress-tests on your system.**

  I'm working on a variety of stress-testing utilities to artificially stress the watcher code in different ways. Making sure that these work for you too would be useful data.

  ```bash
  # Watch <n> simultaneous root directories.
  # This is useful to test the polling fallback.
  $ watcher-stress --exercise parallel \
    --watcher-count 1000 \
    --logging-dir ~/watcher-logs

  # Start and stop <n> watcher roots.
  # This is useful for triggering race conditions in watcher starting and stopping.
  $ watcher-stress --exercise serial \
    --watcher-count 1000 \
    --logging-dir ~/watcher-logs
  ```

* **Try the CLI or stress-tests against different filesystems.**

  If you have access to any sort of non-standard filesystem, I'd love to make sure that @atom/watcher can handle it properly. Specifically, I'm interested in our behaviour on:

  * Network drives on any kind: NAS, Samba shares of various versions
  * FUSE filesystems like `sshfs` or `s3fs`.
  * Older filesystems like HFS+ on MacOS, FAT32 on Windows, anything other than ext3 on Linux.

  Depending on specific filesystem behavior, the expected results can range from full functionality to a fallback to polling to silent failure. We may or may not be able to do something reasonable for each, but having a sense of our behavior would be valuable data.

* **Use it in a project!**

  Writing something in Node.js that would benefit from filesystem watching events? Depend on this and give it a spin. If you already have users, you'll likely want to keep it behind a feature-flag or provide a fallback while we're in this alpha-ish phase.

## What I'm looking for

I'd :heart: an issue if you see any of the following. Please attach the log files from your log directory if you have them. _Notice that the logs will contain paths and usernames; scrub out anything sensitive before you submit._

In rough order of urgency:

1. **Build failures.** Paste me the compiler output and I'll see what I can do.

   Note that installing `@smashwilson/watcher-stress` will report these warnings:

   ```
   gyp WARN download NVM_NODEJS_ORG_MIRROR is deprecated and will be removed in node-gyp v4, please use NODEJS_ORG_MIRROR
    gyp WARN download NVM_NODEJS_ORG_MIRROR is deprecated and will be removed in node-gyp v4, please use NODEJS_ORG_MIRROR
    gyp WARN download NVM_NODEJS_ORG_MIRROR is deprecated and will be removed in node-gyp v4, please use NODEJS_ORG_MIRROR
      CC(target) Debug/obj.target/openpa/openpa/src/opa_primitives.o
      CC(target) Debug/obj.target/openpa/openpa/src/opa_queue.o
      LIBTOOL-STATIC Debug/openpa.a
      CXX(target) Debug/obj.target/nsfw/src/NSFW.o
    ../src/NSFW.cpp:159:37: warning: 'NewInstance' is deprecated [-Wdeprecated-declarations]
        info.GetReturnValue().Set(cons->NewInstance());
                                        ^
    /Users/smashwilson/.node-gyp/8.1.4/include/node/v8.h:3674:3: note: 'NewInstance' has been explicitly marked deprecated here
      V8_DEPRECATED("Use maybe version", Local<Object> NewInstance() const);
      ^
    /Users/smashwilson/.node-gyp/8.1.4/include/node/v8config.h:332:29: note: expanded from macro 'V8_DEPRECATED'
      declarator __attribute__((deprecated))
                                ^
    1 warning generated.
      CXX(target) Debug/obj.target/nsfw/src/Queue.o
      CXX(target) Debug/obj.target/nsfw/src/NativeInterface.o
      CXX(target) Debug/obj.target/nsfw/src/Lock.o
      CXX(target) Debug/obj.target/nsfw/src/osx/RunLoop.o
      CXX(target) Debug/obj.target/nsfw/src/osx/FSEventsService.o
      SOLINK_MODULE(target) Debug/nsfw.node
   ```

   These are from [nsfw](https://github.com/axosoft/nsfw), one of the other watcher backends I test against.

2. **Crashes in native code.** Any kind of crash in C++ land is :fire:. Please collect a stack trace if you can and file me an issue. For example, on MacOS:

   ```bash
   $ ls -l /cores
   # Find the latest core.XYZ file

   $ lldb --core /cores/core.XYZ \
    --one-line 'thread backtrace all' \
    --one-line 'quit'
   ```

3. **Lock-ups in native code.** If the process stops responding entirely, there's probably a race condition somewhere. Logs and stack traces would be most valuable here. (MacOS lets you pull native stack traces from the Activity Monitor, for example.)

4. **Excessive RAM consumption.** Especially if you notice the node process gradually consuming more and more memory as the day goes on. This means that there's a memory leak somewhere, although it will be difficult to track down where without something like [`valgrind`](http://valgrind.org/). If you feel ambitious you can speculate about the cause, but even knowing we have a leak lurking somewhere would be valuable. :smile:

5. **Sustained high CPU usage.** The stress-tests _will_ churn your CPU and disk fairly heavily (because they're stress-tests) but the CLI should not. You may notice brief CLI CPU usage spikes if you clone a large git repository or unpack a big tarball, but if usage stays high or spikes when you aren't doing anything, that's a problem.

6. **Incorrect events.** If you happen to notice @atom/watcher emitting events that don't match what you were actually doing, please let me know. (This is especially likely to happen on MacOS, as FSEvents needs the most heuristics to interpret.)
