load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")
package(default_visibility = ["//visibility:public"])


refresh_compile_commands(
    name = "test_compile_commands",
    targets = {
        "//test:all": "",
    }
)

refresh_compile_commands(
    name = "example_compile_commands",
    targets = {
        "//examples/linux_rdbi_wdbi:all": "",
    }
)

refresh_compile_commands(
    name = "lib_compile_commands",
    targets = {
        "//:iso14229": "",
    }
)

cc_library(
    name = "iso14229",
    srcs=glob(["src/**/*.c", "src/**/*.h"]),
    includes=["src"],
    copts = [
        # gcc adds system headers by default. However, the compile_commands.json used for static analysis needs this include path to be explicit.
        "-I/usr/include",
    ],
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
