package(default_visibility = ["//visibility:public"])


filegroup(
    name = "iso14229_srcs",
    srcs = [
        "iso14229.c",
        "iso14229.h",
        "iso14229serverbufferedwriter.h",
    ],
)

cc_test(
    name="test_all",
    srcs=[
        ":iso14229_srcs",
        "test_iso14229.c",
    ],
    deps = [
        "//tp:mock",
    ],
    copts=[
        "-Wall",
        "-Wextra",
        "-Wno-missing-field-initializers",
        "-Werror",
        "-Wno-unused-parameter",
    ],
    defines=[
        "UDS_TP=UDS_TP_CUSTOM",
        "UDS_CUSTOM_MILLIS",
    ],
    size = "small",
)

cc_test(
    name = "test_bufferedwriter",
    srcs = [
        "iso14229serverbufferedwriter.h",
        "test_iso14229serverbufferedwriter.c",
    ],
    size = "small",
)

cc_library(
    name="iso14229",
    srcs=[":iso14229_srcs"],
)
