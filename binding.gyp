{
    "targets": [{
        "target_name": "sfw",

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
                    "src/worker/platform_macos.cpp"
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
