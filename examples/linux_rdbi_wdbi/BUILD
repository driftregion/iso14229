cc_binary(
    name = "server",
    srcs = [ 
        "server.c",
        "//:iso14229.c",
        "//:iso14229.h",
    ],
    copts = [ "-g" ],
    defines = [
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
        "UDS_TP_ISOTP_SOCK",
        "UDS_LINES",
    ]
)

cc_binary(
    name = "client",
    srcs = [ 
        "client.c" ,
        "//:iso14229.c",
        "//:iso14229.h",
    ],
    copts = [ "-g" ],
    defines = [
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
        "UDS_TP_ISOTP_SOCK",
        "UDS_LINES",
    ]
)

sh_test(
    name = "test",
    srcs = ["test.sh"],
    data = [
        "server",
        "client",
    ],
    args = [
        "$(location :server)",
        "$(location :client)",
    ],
    tags=["vcan"],
)
