package(default_visibility = ["//visibility:public"])

genrule(
    name="gen_version_h",
    srcs=["//:VERSION"],
    outs=["version.h"],
    # #define UDS_VERSION "$(VERSION)"
    cmd="awk '{print \"#define UDS_VERSION \\\"\" $$1 \"\\\"\"}' $< > $@",
)

filegroup(
    name = "sources",
    srcs = [
        "client.c",
        "log.c",
        "server.c",
        "tp.c",
        "util.c",
        "tp/isotp_c_socketcan.c",
        "tp/isotp_c.c",
        "tp/isotp_mock.c",
        "tp/isotp_sock.c",
        "tp/isotp-c/isotp.c",
    ]
)

filegroup(
    name = "headers",
    srcs = [
        "client.h",
        "config.h",
        "log.h",
        "server.h",
        "sys_arduino.h",
        "sys_esp32.h",
        "sys_unix.h",
        "sys_win32.h",
        "sys.h",
        "tp.h",
        "uds.h",
        "util.h",
        "version.h",
        "tp/isotp_c_socketcan.h",
        "tp/isotp_c.h",
        "tp/isotp_mock.h",
        "tp/isotp_sock.h",
        "tp/isotp-c/isotp_config.h",
        "tp/isotp-c/isotp_defines.h",
        "tp/isotp-c/isotp_user.h",
        "tp/isotp-c/isotp.h",
    ]
)
