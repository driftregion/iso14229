package(default_visibility = ["//visibility:public"])

filegroup(
    name = "srcs",
    srcs = [
        "iso14229.c",
        "iso14229.h",
        "iso14229serverbufferedwriter.h",
    ],
)

cc_test(
    name="test",
    srcs=[
        ":srcs",
        "test_iso14229.c",
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
)

cc_test(
    name = "test_server_bufferedwriter",
    srcs = [
        "udsserverbufferedwriter.h",
        "test_udsserverbufferedwriter.c",
    ],
)


filegroup(
    name="isotp_c_srcs",
    srcs=[
        "isotp-c/isotp.c",
        "isotp-c/isotp.h",
        "isotp-c/isotp_config.h",
        "isotp-c/isotp_defines.h",
        "isotp-c/isotp_user.h",
    ],
)

cc_library(
    name="isotp_c",
    srcs=[":isotp_c_srcs"],
    copts=["-Wno-unused-parameter"],
)
