package(default_visibility = ["//visibility:public"])


cc_library(
    name = "cmocka",
    srcs = [
        "include/cmocka.h",
        "include/cmocka_private.h",
        "src/cmocka.c",
    ],
    copts = [
        "-DHAVE_SIGNAL_H",
    ] + select({
        "@bazel_tools//src/conditions:windows": [
            "-DHAVE__SNPRINTF_S",
            "-DHAVE__VSNPRINTF_S",
        ],
        "//conditions:default": [],
    }),
    includes = [
        "include",
    ],
)