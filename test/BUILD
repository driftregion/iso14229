cc_library(
    name = "env",
    srcs = [
        "env.c",
        "env.h",
    ],
    deps = [ 
        "@cmocka",
        "//:iso14229",
    ],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": [ "-g", ],
    })
)

TEST_SRCS = [
    "test_client.c",
    "test_server.c",
]

TEST_NAMES = [ src.split(".c")[0] for src in TEST_SRCS ]

[
    cc_test(
        name=name + "_tp_mock", 
        srcs = [
            src,
        ],
        deps = [ 
            "//:iso14229",
            "@cmocka",
            ":env",
        ],
        size = "small",
        copts = select({
            "@platforms//os:windows": [],
            "//conditions:default": [ "-g", ],
        }),
        defines = [
            "UDS_TP_ISOTP_MOCK",
            "UDS_CUSTOM_MILLIS",
            "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
            "UDS_LINES",
        ],
    ) for name, src in zip(TEST_NAMES, TEST_SRCS)
]


cc_test(
    name = "test_fuzz_server",
    srcs = [
        "test_fuzz_server.c",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    defines = [
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LINES",
    ],
    copts = [
        "-g",
        "-O0",
        "-fsanitize=fuzzer,signed-integer-overflow,address,undefined",
        "-fprofile-instr-generate",
        "-fcoverage-mapping",
    ],
    linkopts = [
        "-fsanitize=fuzzer,signed-integer-overflow,address,undefined",
    ],
    size = "enormous",
    target_compatible_with = ["//platforms/compiler:clang"],
)

cc_library(
    name = "test_prefix_c",
    srcs = [
        "//:iso14229.h",
        "//:iso14229.c",
    ],
)

sh_test(
    name = "test_prefix",
    srcs = ["test_prefix.sh"],
    data = [":test_prefix_c"],
    args = ["$(locations :test_prefix_c)"],
    size = "small",

    # prevent this test from being run under qemu or Windows
    target_compatible_with = [
        "@platforms//cpu:x86_64",
        "@platforms//os:linux",
    ],
)

cc_binary(
    name = "test_log_disable_c",
    srcs = [
        "test_log_disable.c",
        "//src:iso14229.h",
        "//src:iso14229.c",
    ],
    linkopts = ["-Wl,--gc-sections"],
)

# Test that logging is disabled by default
sh_test(
    name = "test_log_disable",
    srcs = ["test_log_disable.sh"],
    data = [":test_log_disable_c"],
    args = ["$(locations :test_log_disable_c)"],
    size = "small",

    target_compatible_with = [
        "@platforms//cpu:x86_64",
        "@platforms//os:linux",
    ],
)

sh_test(
    name = "test_version",
    srcs = ["test_version.sh"],
    args = ["$(locations //src:iso14229.h)"],
    data = [
        "//src:iso14229.h",
        "//:README.md",
    ],
)

cc_library(
    name = "ultra_strict",
    srcs = [
        "//src:iso14229.h",
        "//src:iso14229.c",
    ],
    copts = [
        "-Werror",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wno-unused-parameter",
        "-Wno-gnu-zero-variadic-macro-arguments",
    ],
    defines = [
        "UDS_TP_ISOTP_C_SOCKETCAN",
        "UDS_TP_ISOTP_SOCK",
        "UDS_TP_ISOTP_MOCK",
    ],
    target_compatible_with = ["//platforms/compiler:clang"],
)

cc_binary(
    name = "make_corpus",
    srcs = [
        "make_corpus.c",
        "//src:iso14229.h",
    ],
)

[
    cc_test(
        name = "test_tp_isotp_compliance_" + tp_name,
        srcs = [
            "test_tp_isotp_compliance.c",
        ],
        deps = [ 
            "@cmocka",
            ":env",
        ],
        defines = [
            "UDS_CUSTOM_MILLIS",
            "UDS_LINES",
            "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
            "UDS_TP_ISOTP_MOCK",
            "UDS_TP_ISOTP_SOCK",
            "UDS_TP_ISOTP_C_SOCKETCAN",
        ],
        copts = [ "-g", ],
        size = "small",
        args = [tp_name],
        tags = tags,
        target_compatible_with = ["@platforms//os:linux"],
    )
    for tp_name, tags in [
        ("mock", []),
        ("sock", ["vcan", "exclusive"]),
        ("c", ["vcan", "exclusive"]),
    ]
]

