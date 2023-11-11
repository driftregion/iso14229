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
        name=src.split(".c")[0] , 
        srcs=[src], 
        deps=[":env"],
        size = "small",
        env={
            # "TP": "isotp_sock",
            # "TP": "isotp-c",
            "TP": "mock",
        },
    )
    for src in [
        "test_client_0x11_ECU_reset.c",
        "test_client_0x22_RDBI_tx_buffer_too_small.c",
        "test_client_0x22_RDBI_unpack_response.c",
        "test_client_0x31_RCRRP.c",
        "test_client_0x34_request_download.c",
        "test_client_busy.c",
        "test_client_p2.c",
        "test_client_suppress_positive_response.c",
        "test_server_0x10_diag_sess_ctrl_functional_request.c",
        "test_server_0x10_diag_sess_ctrl_is_disabled_by_default.c",
        "test_server_0x11_no_send_recv_after_ECU_reset.c",
        "test_server_0x22_RDBI.c",
        "test_server_0x23_read_memory_by_address.c",
        "test_server_0x27_security_access.c",
        "test_server_0x31_RCRRP.c",
        "test_server_0x34.c",
        "test_server_0x3E_suppress_positive_response.c",
        "test_server_0x83_diagnostic_session_control.c",
        "test_server_session_timeout.c",
    ]
]


cc_test(
    name = "test_fuzz_server",
    srcs = [
        "test_fuzz_server.c",
        "//:iso14229_srcs",
        "//tp:srcs",
    ],
    defines = [
        "UDS_TP=UDS_TP_CUSTOM",
        "UDS_CUSTOM_MILLIS",
        "UDS_DBG_PRINT=printf",
    ],
    copts = [
        "-g",
        "-O1",
        "-fsanitize=fuzzer,signed-integer-overflow,address,undefined",
        "-fprofile-instr-generate",
        "-fcoverage-mapping",
    ],
    linkopts = [
        "-fsanitize=fuzzer,signed-integer-overflow,address,undefined",
    ],
    size = "enormous",
    target_compatible_with = ["//platforms/compiler:clang"],
)

sh_test(
    name = "test_prefix",
    srcs = ["test_prefix.sh"],
    data = ["//:iso14229"],
    args = ["$(locations //:iso14229)"],
    size = "small",

    # Not exactly right. It's to prevent this test from being run under qemu
    target_compatible_with = ["@platforms//cpu:x86_64"],
)