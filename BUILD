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

cc_library(
    name = "example_host_linux",
    srcs = [
        "example/linux_host.c",
        "example/host.h",
    ],
)

cc_binary(
    name = "example_server",
    srcs = [ "example/server.c" ],
    deps = [ ":server", ":example_host_linux"],
)
