{
    "targets": [{
        "target_name": "watcher",

        "sources": [
            "src/main.cpp",
            "src/log.cpp",
            "src/errable.cpp",
            "src/queue.cpp",
            "src/lock.cpp",
            "src/message.cpp",
            "src/thread.cpp",
            "src/worker/worker_thread.cpp"
        ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "conditions": [
            ["OS=='mac'", {
                "sources": [
                    "src/worker/macos/macos_worker_platform.cpp",
                    "src/worker/macos/recent_file_cache.cpp",
                    "src/worker/macos/event_handler.cpp",
                    "src/worker/macos/rename_buffer.cpp"
                ],
                "link_settings": {
                    "xcode_settings": {
                        "OTHER_LDFLAGS": [
                            "-framework CoreServices"
                        ],
                        "OTHER_CFLAGS": [
                            "-Wno-unknown-pragmas"
                        ]
                    }
                }
            }],
            ["OS=='win'", {
                "sources": [
                    "src/worker/windows/windows_worker_platform.cpp"
                ]
            }],
            ["OS=='linux'", {
                "sources": [
                    "src/worker/linux/linux_worker_platform.cpp"
                ]
            }]
        ],
    }],
    "target_defaults": {
        "cflags_cc": [
            "-std=c++11",
            "-Wall"
        ],
        "conditions": [
            ['OS=="mac"', {
                "xcode_settings": {
                    'CLANG_CXX_LIBRARY': 'libc++',
                    'CLANG_CXX_LANGUAGE_STANDARD':'c++11',
                }
            }]
        ]
    }
}
