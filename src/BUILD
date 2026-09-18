package(default_visibility = ["//visibility:public"])
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_binary")

filegroup(
    name = "sources",
    srcs = [
        "client.c",
        "log.c",
        "server.c",
        "tp.c",
        "util.c",
        "util_private.h",
        "tp/isotp_c_socketcan.c",
        "tp/isotp_c.c",
        "tp/isotp_mock.c",
        "tp/isotp_sock.c",
        "tp/isotp-c/isotp.c",
    ]
)

filegroup(
    name = "headers",
    srcs = [
        "client.h",
        "config.h",
        "log.h",
        "server.h",
        "sys.h",
        "tp.h",
        "uds.h",
        "util.h",
        "version.h",
        "tp/isotp_c_socketcan.h",
        "tp/isotp_c.h",
        "tp/isotp_mock.h",
        "tp/isotp_sock.h",
        "tp/isotp-c/isotp_config.h",
        "tp/isotp-c/isotp_defines.h",
        "tp/isotp-c/isotp_user.h",
        "tp/isotp-c/isotp.h",
    ]
)

genrule(
    name="amalgamated",
    srcs=[
        "//src:sources",
        "//src:headers",
    ],
    outs=["iso14229.c", "iso14229.h"],
    cmd="$(location //tools:amalgamate) --out_c $(location :iso14229.c) --out_h $(location :iso14229.h) $(SRCS)",
    tools=["//tools:amalgamate"],
)

cc_library(
    name="iso14229",
    srcs=[ ":iso14229.c" ],
    hdrs=[ ":iso14229.h" ],
    copts = select({
        "@platforms//os:windows": [
            "/WX",  # Treat warnings as errors (MSVC equivalent of -Werror)
            "/W4",  # High warning level (MSVC equivalent of -Wall -Wextra)
        ],
        "//conditions:default": [ 
            "-g", 
            "-Werror",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wno-gnu-zero-variadic-macro-arguments",
            "-Wno-gnu-zero-line-directive", #makes `#line 0` not an error
            "-Wconversion",
            "-Wsign-conversion",
        ],
    }),
    defines = [
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
        "UDS_LINES",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            "UDS_TP_ISOTP_C_SOCKETCAN",
            "UDS_TP_ISOTP_SOCK",
        ],
    }),
)
