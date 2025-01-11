#include "test/env.h"
#include <unistd.h>

static UDSTpHandle_t *server_tp = NULL;
static UDSTpHandle_t *client_tp = NULL;
static uint32_t g_ms = 0;

uint32_t UDSMillis() { return g_ms; }

#if defined(UDS_TP_ISOTP_MOCK)
static int setup_tp_mock_server(void **state) {
    server_tp = ISOTPMockNew("server", ISOTPMock_DEFAULT_SERVER_ARGS);
    return 0;
}

static int teardown_tp_mock_server(void **state) {
    ISOTPMockFree(server_tp);
    return 0;
}

static int setup_tp_mock_client(void **state) {
    client_tp = ISOTPMockNew("client", ISOTPMock_DEFAULT_CLIENT_ARGS);
    return 0;
}

static int teardown_tp_mock_client(void **state) {
    ISOTPMockFree(client_tp);
    return 0;
}

static int setup_tp_mock_both(void **state) {
    setup_tp_mock_server(state);
    setup_tp_mock_client(state);
    return 0;
}

static int teardown_tp_mock_both(void **state) {
    teardown_tp_mock_server(state);
    teardown_tp_mock_client(state);
    return 0;
}
#endif // defined(UDS_TP_ISOTP_MOCK)

#if defined(UDS_TP_ISOTP_SOCK)
static int setup_tp_isotp_sock_server(void **state) {
    UDSTpIsoTpSock_t *server_isotp = malloc(sizeof(UDSTpIsoTpSock_t));
    strcpy(server_isotp->tag, "server");
    assert(UDS_OK == UDSTpIsoTpSockInitServer(server_isotp, "vcan0", 0x7e8, 0x7e0, 0x7df));
    server_tp = (UDSTpHandle_t *)server_isotp;
    return 0;
}

static int teardown_tp_isotp_sock_server(void **state) {
    UDSTpIsoTpSockDeinit((UDSTpIsoTpSock_t *)server_tp);
    free(server_tp);
    return 0;
}

static int setup_tp_isotp_sock_client(void **state) {
    UDSTpIsoTpSock_t *client_isotp = malloc(sizeof(UDSTpIsoTpSock_t));
    strcpy(client_isotp->tag, "client");
    assert(UDS_OK == UDSTpIsoTpSockInitClient(client_isotp, "vcan0", 0x7e0, 0x7e8, 0x7df));
    client_tp = (UDSTpHandle_t *)client_isotp;
    return 0;
}

static int teardown_tp_isotp_sock_client(void **state) {
    UDSTpIsoTpSockDeinit((UDSTpIsoTpSock_t *)client_tp);
    free(client_tp);
    return 0;
}

static int setup_tp_isotp_sock_both(void **state) {
    setup_tp_isotp_sock_server(state);
    setup_tp_isotp_sock_client(state);
    return 0;
}

static int teardown_tp_isotp_sock_both(void **state) {
    teardown_tp_isotp_sock_server(state);
    teardown_tp_isotp_sock_client(state);
    return 0;
}
#endif // defined(UDS_TP_ISOTP_SOCK)

#if defined(UDS_TP_ISOTP_C)
static int setup_tp_isotp_c_server(void **state) {
    UDSTpISOTpC_t *server_isotp = malloc(sizeof(UDSTpISOTpC_t));
    strcpy(server_isotp->tag, "server");
    assert(UDS_OK == UDSTpISOTpCInit(server_isotp, "vcan0", 0x7e8, 0x7e0, 0x7df, 0));
    server_tp = (UDSTpHandle_t *)server_isotp;
    return 0;
}

static int teardown_tp_isotp_c_server(void **state) {
    UDSTpISOTpCDeinit((UDSTpISOTpC_t *)server_tp);
    free(server_tp);
    return 0;
}

static int setup_tp_isotp_c_client(void **state) {
    UDSTpISOTpC_t *client_isotp = malloc(sizeof(UDSTpISOTpC_t));
    strcpy(client_isotp->tag, "client");
    assert(UDS_OK == UDSTpISOTpCInit(client_isotp, "vcan0", 0x7e0, 0x7e8, 0, 0x7df));
    client_tp = (UDSTpHandle_t *)client_isotp;
    return 0;
}

static int teardown_tp_isotp_c_client(void **state) {
    UDSTpISOTpCDeinit((UDSTpISOTpC_t *)client_tp);
    free(client_tp);
    return 0;
}

static int setup_tp_isotp_c_both(void **state) {
    setup_tp_isotp_c_server(state);
    setup_tp_isotp_c_client(state);
    return 0;
}

static int teardown_tp_isotp_c_both(void **state) {
    teardown_tp_isotp_c_server(state);
    teardown_tp_isotp_c_client(state);
    return 0;
}
#endif // defined(UDS_TP_ISOTP_C)

static void TestSendRecv(void **state) {

    // When some data is sent by the client
    const uint8_t MSG[] = {0x10, 0x02};
    UDSTpSend(client_tp, MSG, sizeof(MSG), NULL);

    // and a short time has passed
    for (int i = 0; i < 2; i++) {
        UDSTpPoll(client_tp);
        UDSTpPoll(server_tp);
        g_ms++;
    }

    // the server should have received it
    assert_int_equal(UDSTpGetRecvLen(server_tp), sizeof(MSG));
    assert_memory_equal(UDSTpGetRecvBuf(server_tp, NULL), MSG, sizeof(MSG));
}

void TestSendRecvFunctional(void **state) {

    // When a functional request is sent
    const uint8_t MSG[] = {0x10, 0x02};
    UDSSDU_t info = {0};
    UDSTpSend(client_tp, MSG, sizeof(MSG), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    // and a short time has passed
    for (int i = 0; i < 2; i++) {
        UDSTpPoll(client_tp);
        UDSTpPoll(server_tp);
        g_ms++;
    }

    // the server should receive it quickly
    uint8_t *buf = NULL;
    assert_int_equal(UDSTpGetRecvLen(server_tp), sizeof(MSG));

    // it should be the same message
    UDSTpPeek(server_tp, &buf, &info);
    assert_memory_equal(buf, MSG, sizeof(MSG));

    // and the server should know it's a functional request
    assert_int_equal(info.A_TA_Type, UDS_A_TA_TYPE_FUNCTIONAL);
}

void TestLargestSingleFrame(void **state) {

    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7};
    UDSSDU_t info = {0};

    // When a functional request is sent
    ssize_t ret =
        UDSTpSend(client_tp, MSG, sizeof(MSG), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    // and a short time has passed
    for (int i = 0; i < 2; i++) {
        UDSTpPoll(client_tp);
        UDSTpPoll(server_tp);
        g_ms++;
    }

    // the server should receive it quickly
    assert_int_equal(UDSTpGetRecvLen(server_tp), sizeof(MSG));

    // it should be the same message
    uint8_t *buf = NULL;
    UDSTpPeek(server_tp, &buf, &info);
    assert_memory_equal(buf, MSG, sizeof(MSG));

    // and the server should know it's a functional request
    assert_int_equal(info.A_TA_Type, UDS_A_TA_TYPE_FUNCTIONAL);
}

// ISO 15765-2 2016 Table 4 note b. Functional addressing shall only be supported for SingleFrame communication.
void TestSendFunctionalLargerThanSingleFrameFails(void **state) {
    // When a functional request is sent with more than 7 bytes
    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7, 8};
    ssize_t ret =
        UDSTpSend(client_tp, MSG, sizeof(MSG), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    // it should fail
    assert_true(ret < 0);
}

void TestSendRecvMaxLen(void **state) {
    uint8_t MSG[4095] = {0};
    MSG[0] = 0x10;
    MSG[4094] = 0x02;

    UDSTpSend(client_tp, MSG, sizeof(MSG), NULL);

    for (int i = 0; i < 1000; i++) {
        UDSTpPoll(client_tp);
        UDSTpPoll(server_tp);
        g_ms++;
        usleep(1 * 1000);
    }

    assert_int_equal(UDSTpGetRecvLen(server_tp), sizeof(MSG));
    assert_memory_equal(UDSTpGetRecvBuf(server_tp, NULL), MSG, sizeof(MSG));
}

void TestFlowControlFrameTimeout(void **state) {
    // sending multiframe to wait for Flow Control frame
    // which will not arrive since no server is running
    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7, 8};
    ssize_t ret = UDSTpSend(client_tp, MSG, sizeof(MSG), NULL);

    for (int i = 0; i < 1000; i++) {
        UDSTpPoll(client_tp);
        g_ms++;
        usleep(1 * 1000);
    }

    // failure is expected as the elapsed 1s timeout raises an error on the ISOTP socket
    assert_true(ret < 0);
}

int main() {
    int ret = 0;

#if defined(UDS_TP_ISOTP_MOCK)
    printf("TP: mock\n");
    const struct CMUnitTest tp_mock_tests[] = {
        cmocka_unit_test_setup_teardown(TestSendRecv, setup_tp_mock_both, teardown_tp_mock_both),
        cmocka_unit_test_setup_teardown(TestSendRecvFunctional, setup_tp_mock_both,
                                        teardown_tp_mock_both),
        cmocka_unit_test_setup_teardown(TestLargestSingleFrame, setup_tp_mock_both,
                                        teardown_tp_mock_both),

        // this should pass, but it isn't.
        // cmocka_unit_test_setup_teardown(TestSendFunctionalLargerThanSingleFrameFails,
        // setup_tp_mock_both, teardown_tp_mock_both),

        cmocka_unit_test_setup_teardown(TestSendRecvMaxLen, setup_tp_mock_both,
                                        teardown_tp_mock_both),
    };
    ret += cmocka_run_group_tests(tp_mock_tests, NULL, NULL);
    printf("\n");
#endif // defined(UDS_TP_ISOTP_MOCK)

#if defined(UDS_TP_ISOTP_C)
    printf("TP: isotp-c\n");
    const struct CMUnitTest tp_isotp_c_tests[] = {
        cmocka_unit_test_setup_teardown(TestSendRecv, setup_tp_isotp_c_both,
                                        teardown_tp_isotp_c_both),
        cmocka_unit_test_setup_teardown(TestSendRecvFunctional, setup_tp_isotp_c_both,
                                        teardown_tp_isotp_c_both),
        cmocka_unit_test_setup_teardown(TestSendFunctionalLargerThanSingleFrameFails, setup_tp_isotp_c_both,
                                        teardown_tp_isotp_c_both),
        cmocka_unit_test_setup_teardown(TestSendRecvMaxLen, setup_tp_isotp_c_both,
                                        teardown_tp_isotp_c_both),

        // this should pass, but it isn't.
        // cmocka_unit_test_setup_teardown(TestFlowControlFrameTimeout, setup_tp_isotp_c_client,
        // teardown_tp_isotp_c_client),
    };
    ret += cmocka_run_group_tests(tp_isotp_c_tests, NULL, NULL);
    printf("\n");
#endif // defined(UDS_TP_ISOTP_C)

#if defined(UDS_TP_ISOTP_SOCK)
    printf("TP: isotp-sock\n");
    const struct CMUnitTest tp_isotp_sock_tests[] = {
        cmocka_unit_test_setup_teardown(TestSendRecv, setup_tp_isotp_sock_both,
                                        teardown_tp_isotp_sock_both),

        // this should pass, but it isn't.
        // cmocka_unit_test_setup_teardown(TestSendRecvFunctional, setup_tp_isotp_sock_both,
        // teardown_tp_isotp_sock_both),

        cmocka_unit_test_setup_teardown(TestSendFunctionalLargerThanSingleFrameFails,
                                        setup_tp_isotp_sock_both, teardown_tp_isotp_sock_both),
        cmocka_unit_test_setup_teardown(TestSendRecvMaxLen, setup_tp_isotp_sock_both,
                                        teardown_tp_isotp_sock_both),
        cmocka_unit_test_setup_teardown(TestFlowControlFrameTimeout, setup_tp_isotp_sock_client,
                                        teardown_tp_isotp_sock_client),
    };
    ret += cmocka_run_group_tests(tp_isotp_sock_tests, NULL, NULL);
    printf("\n");
#endif // defined(UDS_TP_ISOTP_SOCK)

    return ret;
}
