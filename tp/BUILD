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
    name="isotp_c",
    srcs=[
        "isotp_c.h",
        "isotp.h",
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
    name="srcs",
    # srcs=glob(["*.c", "*.h"])
    srcs = [
        "mock.c",
        "mock.h",
        "isotp_sock.c",
        "isotp_sock.h",
    ]
)


cc_binary(
    name="echo",
    srcs = [
        "echo.c",
    ],
)