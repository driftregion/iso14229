load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")
package(default_visibility = ["//visibility:public"])
exports_files(["README.md"])

refresh_compile_commands(
    name = "test_compile_commands",
    targets = {
        "//test:all": "",
    }
)

refresh_compile_commands(
    name = "example_compile_commands",
    targets = {
        "//examples/linux_rdbi_wdbi:all": "",
    }
)

refresh_compile_commands(
    name = "lib_compile_commands",
    targets = {
        "//:iso14229_unamalgamated": "",
    }
)

cc_library(
    name = "iso14229_unamalgamated",
    srcs=[
        "//src:sources",
        "//src:headers",
    ],
    includes=["src"],
    copts = [
        # gcc adds system headers by default. However, the compile_commands.json used for static analysis needs this include path to be explicit.
        "-I/usr/include",
    ]
)

cc_binary(
    name = "libiso14229_unamalgamated.so",
    srcs=[
        "//src:sources",
        "//src:headers",
    ],
    includes=["src"],
    copts = [
        # gcc adds system headers by default. However, the compile_commands.json used for static analysis needs this include path to be explicit.
        "-I/usr/include",
        # https://github.com/lvc/abi-dumper requires
        "-g",
        "-Og",
        "-fno-eliminate-unused-debug-types",
        "-fPIC",
    ],
    linkshared = 1,
)

cc_library(
    name="iso14229",
    srcs=[
        "//src:iso14229.c",
    ],
    hdrs=[
        "//src:iso14229.h",
    ],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": [ "-g", ],
    }),
    defines = [
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [ 
            "UDS_TP_ISOTP_C_SOCKETCAN",
            "UDS_TP_ISOTP_SOCK",
        ],
    }),
)

genrule(
    name="release",
    srcs=[
        "iso14229.c",
        "iso14229.h",
        "README.md",
        "LICENSE",
        "VERSION",
        "AUTHORS.txt",
    ],
    outs = ["iso14229.zip"],
    cmd = "mkdir iso14229 && cp -L $(SRCS) iso14229/ && zip -r $(OUTS) iso14229",
)

genrule(
    name = "gen_version_txt",
    outs = ["VERSION"],
    stamp = 1,
    cmd = "python tools/gen_version.py bazel-out/stable-status.txt $(OUTS)",
)