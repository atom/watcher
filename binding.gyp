{
    "targets": [{
        "target_name": "watcher",
        "sources": [
            "src/binding.cpp",
            "src/hub.cpp",
            "src/log.cpp",
            "src/errable.cpp",
            "src/queue.cpp",
            "src/lock.cpp",
            "src/message.cpp",
            "src/message_buffer.cpp",
            "src/thread_starter.cpp",
            "src/thread.cpp",
            "src/status.cpp",
            "src/worker/worker_thread.cpp",
            "src/worker/recent_file_cache.cpp",
            "src/polling/directory_record.cpp",
            "src/polling/polled_root.cpp",
            "src/polling/polling_iterator.cpp",
            "src/polling/polling_thread.cpp",
            "src/helper/libuv.cpp",
            "src/nan/async_callback.cpp",
            "src/nan/all_callback.cpp",
            "src/nan/functional_callback.cpp",
            "src/nan/options.cpp"
        ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "conditions": [
            ["OS=='mac'", {
                "defines": [
                    'PLATFORM_MACOS'
                ],
                "sources": [
                    "src/helper/common_posix.cpp",
                    "src/helper/macos/helper.cpp",
                    "src/worker/macos/macos_worker_platform.cpp",
                    "src/worker/macos/batch_handler.cpp",
                    "src/worker/macos/rename_buffer.cpp",
                    "src/worker/macos/subscription.cpp"
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
                },
                "xcode_settings": {
                    'MACOSX_DEPLOYMENT_TARGET': '10.9'
                }
            }],
            ["OS=='win'", {
                "defines": [
                    'PLATFORM_WINDOWS'
                ],
                "sources": [
                    "src/helper/common_win.cpp",
                    "src/helper/windows/helper.cpp",
                    "src/worker/windows/subscription.cpp",
                    "src/worker/windows/windows_worker_platform.cpp"
                ]
            }],
            ["OS=='linux'", {
                "defines": [
                    'PLATFORM_LINUX'
                ],
                "sources": [
                    "src/helper/common_posix.cpp",
                    "src/worker/linux/pipe.cpp",
                    "src/worker/linux/side_effect.cpp",
                    "src/worker/linux/cookie_jar.cpp",
                    "src/worker/linux/watched_directory.cpp",
                    "src/worker/linux/watch_registry.cpp",
                    "src/worker/linux/linux_worker_platform.cpp"
                ]
            }]
        ],
        "configurations": {
            "Debug": {
                "msvs_settings": {
                    "VCCLCompilerTool": {
                        "ExceptionHandling": "2",
                        "WarningLevel": "3",
                        "DebugInformationFormat": "3",
                        "Optimization": "0"
                    }
                }
            },
            "Release": {
                "msvs_settings": {
                    "VCCLCompilerTool": {
                        "ExceptionHandling": "2",
                        "WarningLevel": "3",
                        "Optimization": "3"
                    }
                }
            }
        }
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
                    'CLANG_CXX_LANGUAGE_STANDARD': 'c++11'
                }
            }]
        ]
    }
}
