package(default_visibility = ["//visibility:public"])

cc_library(
    name="isotp",
    srcs = [
        "isotp-c/isotp.c",
        "isotp-c/isotp.h",
        "isotp-c/isotp_config.h",
        "isotp-c/isotp_defines.h",
    ],
    copts = ["-Wno-unused-parameter"],
)

cc_library(
    name = "server",
    srcs = [
        "iso14229.h",
        "iso14229server.c",
        "iso14229server.h",
        "iso14229serverconfig.h",
        "iso14229serverbufferedwriter.h",
    ],
    deps = [":isotp"],
)

cc_test(
    name = "test_server_bufferedwriter",
    srcs = [
        "iso14229serverbufferedwriter.h",
        "test_iso14229serverbufferedwriter.c",
    ],
)

cc_library(
    name = "client",
    srcs = [
        "iso14229.h",
        "iso14229client.h",
        "iso14229client.c",
    ],
    deps = [":isotp"],
)

cc_test(
    name = "test",
    srcs = [
        "test_iso14229.h",
        "test_iso14229.c",
    ],
    deps=[":client", ":server"],
    copts = ["-Wall", "-Wextra", "-Werror"],
)

filegroup(
    name="example_srcs",
    srcs = [
        "examples/server.c",
        "examples/server.h",
        "examples/client.c",
        "examples/client.h",
        "examples/shared.h",
        "examples/port.h",
    ]
)

cc_binary(
    name = "example",
    srcs = [ 
        ":example_srcs",
        "examples/port_socketcan.c",
        "examples/main.c",
    ],
    deps = [ ":server", ":client"],
)

cc_test(
    name = "test_example",
    srcs = [
        ":example_srcs",
        "examples/port_socketcan.c",
        "examples/test_example.c",
    ],
    deps = [ ":server", ":client"],
    copts = ["-Wall", "-Wextra", "-Werror"],
)
