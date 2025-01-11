cc_library(
    name = "env",
    srcs = [
        "env.c",
        "env.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_TP_ISOTP_C_SOCKETCAN",
        "UDS_TP_ISOTP_SOCK",
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
        "UDS_LINES",
    ],
    copts = [ "-g", ],
)

TEST_SRCS = [
    "test_client_0x11_ECU_reset.c",
    "test_client_0x31_RCRRP.c",
    "test_client_0x34_request_download.c",
    "test_client_0x38_request_file_transfer.c",
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
    "test_server_0x38_request_file_transfer.c",
    "test_server_0x3E_suppress_positive_response.c",
    "test_server_0x83_diagnostic_session_control.c",
    "test_server_fn_misuse.c",
    "test_server_session_timeout.c",
]

TEST_NAMES = [ src.split(".c")[0] for src in TEST_SRCS ]

[
    cc_test(
        name=name + "_tp_mock", 
        srcs = [
            src,
            "env.c",
            "env.h",
            "//:iso14229.h",
            "//:iso14229.c",
        ],
        deps = [ "@cmocka" ],
        size = "small",
        copts = ["-g"],
        defines = [
            "UDS_TP_ISOTP_MOCK",
            "UDS_CUSTOM_MILLIS",
            "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
            "UDS_LINES",
        ],
    ) for name, src in zip(TEST_NAMES, TEST_SRCS)
]


cc_test(
    name = "test_fuzz_server",
    srcs = [
        "test_fuzz_server.c",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    defines = [
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LINES",
    ],
    copts = [
        "-g",
        "-O0",
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

cc_binary(
    name = "test_log_disable_c",
    srcs = [
        "test_log_disable.c",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    linkopts = ["-Wl,--gc-sections"],
)

# Test that logging is disabled by default
sh_test(
    name = "test_log_disable",
    srcs = ["test_log_disable.sh"],
    data = [":test_log_disable_c"],
    args = ["$(locations :test_log_disable_c)"],
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
        "-Wno-gnu-zero-variadic-macro-arguments",
    ],
    defines = [
        "UDS_TP_ISOTP_C_SOCKETCAN",
        "UDS_TP_ISOTP_SOCK",
        "UDS_TP_ISOTP_MOCK",
    ],
    target_compatible_with = ["//platforms/compiler:clang"],
)


cc_binary(
    name = "test_client_sequence",
    srcs = [
        "test_client_sequence.c",
        "env.c",
        "env.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_TP_ISOTP_C_SOCKETCAN",
        "UDS_TP_ISOTP_SOCK",
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
    ],
    copts = [ "-g", ],
)

cc_binary(
    name = "test_client_sequence_2",
    srcs = [
        "test_client_sequence_2.c",
        "env.c",
        "env.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_TP_ISOTP_C_SOCKETCAN",
        "UDS_TP_ISOTP_SOCK",
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
    ],
    copts = [ "-g", ],
)

cc_binary(
    name = "test_client_sequence_3",
    srcs = [
        "test_client_sequence_3.c",
        "env.c",
        "env.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_TP_ISOTP_C_SOCKETCAN",
        "UDS_TP_ISOTP_SOCK",
        "UDS_TP_ISOTP_MOCK",
        "UDS_CUSTOM_MILLIS",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
    ],
    copts = [ "-g", ],
)

cc_binary(
    name = "make_corpus",
    srcs = [
        "make_corpus.c",
        "//:iso14229.h",
    ],
)

cc_test(
    name = "test_tp_isotp_mock_compliance",
    srcs = [
        "test_tp_isotp_compliance.c",
        "env.c",
        "env.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_CUSTOM_MILLIS",
        "UDS_LINES",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
        "UDS_TP_ISOTP_MOCK",
    ],
    copts = [ "-g", ],
    size = "small",
)

cc_test(
    name = "test_tp_isotp_sock_compliance",
    srcs = [
        "test_tp_isotp_compliance.c",
        "env.c",
        "env.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_CUSTOM_MILLIS",
        "UDS_LINES",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
        "UDS_TP_ISOTP_SOCK",
    ],
    copts = [ "-g", ],
    tags = [ 
        "vcan",
        "exclusive",
    ],
    size = "small",
)


cc_test(
    name = "test_tp_isotp_c_socketcan_compliance",
    srcs = [
        "test_tp_isotp_compliance.c",
        "env.c",
        "env.h",
        "//:iso14229.h",
        "//:iso14229.c",
    ],
    deps = [ "@cmocka" ],
    defines = [
        "UDS_CUSTOM_MILLIS",
        "UDS_LINES",
        "UDS_LOG_LEVEL=UDS_LOG_VERBOSE",
        "UDS_TP_ISOTP_C_SOCKETCAN",
    ],
    copts = [ "-g", ],
    tags = [ 
        "vcan",
        "exclusive",
    ],
    size = "small",
)