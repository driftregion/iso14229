package(default_visibility = ["//visibility:public"])
load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")

genrule(
    name = "isotp_c_wrapped_c",
    srcs = glob(["src/tp/isotp-c/*.c"]),
    outs = ["isotp_c_wrapped.c"],
    cmd = "echo '#if defined(UDS_ISOTP_C)' >> $(OUTS) ; for f in $(SRCS); do cat $$f >> $(OUTS); done ; echo '#endif' >> $(OUTS)",
)

genrule(
    name = "isotp_c_wrapped_h",
    srcs = glob(["src/tp/isotp-c/*.h"]),
    outs = ["isotp_c_wrapped.h"],
    cmd = "echo '#if defined(UDS_ISOTP_C)' >> $(OUTS) ; for f in $(SRCS); do cat $$f >> $(OUTS); done ; echo '#endif' >> $(OUTS)",
)

genrule(
    name = "c_src",
    srcs = glob(["src/*.c", "src/tp/*.c"]) + [
        ":isotp_c_wrapped_c",
    ],
    outs = ["iso14229.c"],
    cmd = "(echo '#include \"iso14229.h\"'; (for f in $(SRCS); do echo; echo '#ifdef UDS_LINES'; echo \"#line 1 \\\"$$f\"\\\"; echo '#endif'; cat $$f | sed -e 's,#include \".*,,'; done)) > $(OUTS)",
)

genrule(
    name = "h_src",
    srcs = glob(["src/*.h", "src/tp/*.h"]) + [
            ":isotp_c_wrapped_h",
        ],
    outs = ["iso14229.h"],
    cmd = "echo $(SRCS); (cat ; echo '#ifndef ISO14229_H'; echo '#define ISO14229_H'; echo; echo '#ifdef __cplusplus'; echo 'extern \"C\" {'; echo '#endif'; cat src/sys.h src/sys_arduino.h src/sys_unix.h src/sys_win32.h src/sys_esp32.h src/config.h src/util.h src/tp.h src/uds.h src/client.h src/server.h $(location //:isotp_c_wrapped_h) src/tp/*.h |sed -e 's,#include \".*,,' -e 's,^#pragma once,,' ; echo '#endif'; echo '#ifdef __cplusplus'; echo '}'; echo '#endif';) > $(OUTS)",
)

filegroup(
    name = "srcs",
    srcs = [
        "iso14229.c",
        "iso14229.h",
    ],
)

cc_library(
    name="iso14229",
    srcs = [
        "iso14229.c",
        "iso14229.h",
    ],
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
    name="iso14229_2",
    srcs=glob(["src/**/*.c", "src/**/*.h"]),
    copts=['-Isrc'],
)


refresh_compile_commands(
    name = "s32k_refresh_compile_commands",
    targets = {
        "//examples/s32k144/...": "--config=s32k",
    }
)