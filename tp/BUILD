package(default_visibility = ["//visibility:public"])

cc_library(
    name="mock",
    srcs=[
        "mock.c",
        "mock.h",
        "//:iso14229.h"
    ],
)

cc_library(
    name="isotp_sock",
    srcs=[
        "isotp_sock.c",
        "isotp_sock.h",
        "//:iso14229.h"
    ],
)


filegroup(
    name="isotp_c_srcs",
    srcs=[
        "isotp-c/isotp.c",
        "isotp-c/isotp.h",
        "isotp-c/isotp_config.h",
        "isotp-c/isotp_defines.h",
    ],
)

cc_library(
    name="isotp_c",
    srcs=[":isotp_c_srcs"],
    copts=["-Wno-unused-parameter"],
)

cc_library(
    name="isotp_c_socketcan",
    srcs=[
        "isotp_c_socketcan.c",
        "isotp_c_socketcan.h",
        "//:iso14229.h",
    ],
    deps = [
        ":isotp_c",
    ],
)

filegroup(
    name="srcs",
    srcs = [
        "mock.c",
        "mock.h",
        "isotp_sock.c",
        "isotp_sock.h",
        "isotp_c_socketcan.c",
        "isotp_c_socketcan.h",
        ":isotp_c_srcs",
    ]
)
