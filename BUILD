load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")
package(default_visibility = ["//visibility:public"])

cc_library(
    name="iso14229",
    srcs = [
        "iso14229.c",
        "iso14229.h",
    ],
    defines = select({
        "@platforms//os:linux": ["UDS_TP_ISOTP_SOCK"],
        "//conditions:default": [],
    })
)

refresh_compile_commands(
    name = "s32k_refresh_compile_commands",
    targets = {
        "//examples/s32k144/...": "--config=s32k",
    }
)

refresh_compile_commands(
    name = "test_compile_commands",
    targets = {
        "//test:all": "",
    }
)

py_binary(
    name="amalgamate",
    srcs=["amalgamate.py"],
)

genrule(
    name="amalgamated",
    srcs=glob(["src/**/*.c", "src/**/*.h"]),
    outs=["iso14229.c", "iso14229.h"],
    cmd="$(location //:amalgamate) --out_c $(location //:iso14229.c) --out_h $(location //:iso14229.h) $(SRCS)",
    tools=["//:amalgamate"],
)

genrule(
    name="release",
    srcs=[
        "iso14229.c",
        "iso14229.h",
        "README.md",
        "LICENSE",
        "AUTHORS.txt",
    ],
    outs = ["iso14229.zip"],
    cmd = "mkdir iso14229 && cp -L $(SRCS) iso14229/ && zip -r $(OUTS) iso14229",
)