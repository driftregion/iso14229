cc_library(
    name = "env",
    srcs = [
        "env.c",
        "env.h",
        "test.h",
        "//:iso14229_srcs",
        "//tp:srcs",
    ],
    deps = [
        "//tp:mock",
        "//tp:isotp_sock",
        "//tp:isotp_c_socketcan",
    ],
    defines = [
        "UDS_TP=UDS_TP_CUSTOM",
        "UDS_CUSTOM_MILLIS",
        "UDS_DBG_PRINT=printf",
    ]
)

[
    cc_test(
        name=n.split(".c")[0] , 
        srcs=[n], 
        deps=[":env"],
        size = "small",
        env={
            # "TP": "isotp_sock",
            # "TP": "isotp-c",
            "TP": "mock",
        }
    )

    for n in glob(["test_*.c"])
]


sh_test(
    name = "test_prefix",
    srcs = ["test_prefix.sh"],
    data = ["//:iso14229"],
    args = ["$(locations //:iso14229)"],
)