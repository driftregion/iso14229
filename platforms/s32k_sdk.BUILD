package(default_visibility = ["//visibility:public"])


filegroup(
    name = "srcs",
    srcs = glob(["**/**"]),
)

cc_library(
    name = "include",
    includes = [
        "platform/devices",
        "platform/devices/common",
        "platform/devices/S32K144/startup",
        "platform/drivers/inc",
        "platform/drivers/src/lpuart",
        "rtos/osif",
        "platform/pal/inc",
    ],
)