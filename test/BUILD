cc_library(
    name = "env",
    srcs = [
        "env.c",
        "env.h",
        "test.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_TP_ISOTP_C_SOCKETCAN",
        "UDS_TP_ISOTP_SOCK",
        "UDS_TP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_ENABLE_DBG_PRINT",
        "UDS_ENABLE_ASSERT",
        "UDS_LINES",
    ],
    copts = [ "-g", ],
)

TEST_SRCS = [
    "test_client_0x11_ECU_reset.c",
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
    "test_tp_compliance.c",
]

TEST_NAMES = [ src.split(".c")[0] for src in TEST_SRCS ]

# Generic unit tests
# These should pass on all architectures and transports
[
    cc_test(
        name=name, 
        srcs=[src], 
        deps=[":env"],
        size = "small",
        env = {
            "UDS_TP_TYPE": "0",
        },
        copts = ["-g"],
        tags = [
            "exclusive",
        ],
    )
    for name, src in zip(TEST_NAMES, TEST_SRCS)
]

py_test(
    name = "test_matrix",
    srcs = ["test_matrix.py"],
    size = "small",
    data = TEST_NAMES,
    args = TEST_NAMES,
    tags = [
        "exclusive",
    ],
)

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
    # "UDS_DBG_PRINT=printf",
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

cc_library(
    name = "ultra_strict",
    srcs = [
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    copts = [
        "-Werror",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wno-unused-parameter",
        "-Wno-unused-function",
        "-Wno-unused-variable",
        "-Wno-unused-label",
        "-Wno-unused-value",
        "-Wno-unused-result",
        "-Wno-unused-macros",
    ],
    target_compatible_with = ["//platforms/compiler:clang"],
)