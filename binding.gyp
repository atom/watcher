{
    "targets": [{
        "target_name": "sfw",

        "sources": [
            "src/main.cpp"
        ],
        "win_delay_load_hook": "false",
        "include_dirs": [
            "<!(node -e \"require('nan')\")",
            "includes"
        ],
        "conditions": [
            ["OS=='mac'", {
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
            ["OS=='linux'", {
                "cflags": [
                    "-Wno-unknown-pragmas",
                    "-std=c++0x"
                ]
            }],
            ["OS=='win'", {
                "conditions": [
                    ["target_arch=='x64'", {
                        "VCLibrarianTool": {
                          "AdditionalOptions": [
                            "/MACHINE:X64",
                          ],
                        },
                    }, {
                        "VCLibrarianTool": {
                          "AdditionalOptions": [
                            "/MACHINE:x86",
                          ],
                        },
                    }],
                ]
            }],
            ["OS=='mac' or OS=='linux'", {
                "defines": [
                    "HAVE_STDDEF_H=1",
                    "HAVE_STDLIB_H=1",
                    "HAVE_UNISTD_H=1"
                ]
            }]
        ],
    }]
}
