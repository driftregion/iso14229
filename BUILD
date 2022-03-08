package(default_visibility = ["//visibility:public"])

cc_library(
    name="isotp",
    srcs = [
        "isotp-c/isotp.c",
        "isotp-c/isotp.h",
        "isotp-c/isotp_config.h",
        "isotp-c/isotp_defines.h",
    ],
)

cc_library(
    name = "server",
    srcs = [
        "iso14229.h",
        "iso14229server.c",
        "iso14229server.h",
        "iso14229serverconfig.h",
        "iso14229serverappsoftware.c",
        "iso14229serverappsoftware.h",
        "iso14229serverbootsoftware.c",
        "iso14229serverbootsoftware.h",
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
)
