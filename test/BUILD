load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test", "cc_binary")

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

cc_test(
    name="test_server", 
    srcs = [
        "test_server.c",
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
)

cc_test(
    name="test_client", 
    srcs = [
        "test_client.c",
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
)

sh_test(
    name = "test_version",
    srcs = ["test_version.sh"],
    args = ["$(locations //src:iso14229.h)"],
    data = [
        "//src:iso14229.h",
        "//:CHANGELOG",
        "//:Doxyfile",
    ],
)

cc_library(
    name = "ultra_strict",
    srcs = [
        "//src:iso14229.h",
        "//src:iso14229.c",
    ],
    copts = select({
        "@platforms//os:windows": [
            "/WX",  # Treat warnings as errors (MSVC equivalent of -Werror)
            "/W4",  # High warning level (MSVC equivalent of -Wall -Wextra)
        ],
        "//conditions:default": [
            "-Werror",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wno-unused-parameter",
            "-Wno-gnu-zero-variadic-macro-arguments",
        ],
    }),
    defines = select({
        "@platforms//os:windows": [
            "UDS_TP_ISOTP_MOCK",
        ],
        "//conditions:default": [
            "UDS_TP_ISOTP_C_SOCKETCAN",
            "UDS_TP_ISOTP_SOCK",
            "UDS_TP_ISOTP_MOCK",
        ],
    }),
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

