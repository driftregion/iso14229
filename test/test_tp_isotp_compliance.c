#include "test/env.h"
#include <unistd.h>


int SetupMockTpPair(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    env->server_tp = ISOTPMockNew("server", ISOTPMock_DEFAULT_SERVER_ARGS);
    env->client_tp = ISOTPMockNew("client", ISOTPMock_DEFAULT_CLIENT_ARGS);
    *state = env;
    return 0;
}

int TeardownMockTpPair(void **state) {
    Env_t *env = *state;
    ISOTPMockFree(env->server_tp);
    ISOTPMockFree(env->client_tp);
    ISOTPMockReset();
    free(env);
    return 0;
}

int SetupMockTpClientOnly(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    env->client_tp = ISOTPMockNew("client", ISOTPMock_DEFAULT_CLIENT_ARGS);
    *state = env;
    return 0;
}

int TeardownMockTpClientOnly(void **state) {
    Env_t *env = *state;
    ISOTPMockFree(env->client_tp);
    ISOTPMockReset();
    free(env);
    return 0;
}

int SetupIsoTpCPair(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    UDSTpISOTpC_t *server_isotp = malloc(sizeof(UDSTpISOTpC_t));
    strcpy(server_isotp->tag, "server");
    assert(UDS_OK == UDSTpISOTpCInit(server_isotp, "vcan0", 0x7e8, 0x7e0, 0x7df, 0));
    env->server_tp = (UDSTpHandle_t *)server_isotp;

    UDSTpISOTpC_t *client_isotp = malloc(sizeof(UDSTpISOTpC_t));
    strcpy(client_isotp->tag, "client");
    assert(UDS_OK == UDSTpISOTpCInit(client_isotp, "vcan0", 0x7e0, 0x7e8, 0, 0x7df));
    env->client_tp = (UDSTpHandle_t *)client_isotp;

    *state = env;
    return 0;
}

int TeardownIsoTpCPair(void **state) {
    Env_t *env = *state;
    UDSTpISOTpCDeinit((UDSTpISOTpC_t *)env->server_tp);
    UDSTpISOTpCDeinit((UDSTpISOTpC_t *)env->client_tp);
    free(env->server_tp);
    free(env->client_tp);
    free(env);
    return 0;
}

int SetupIsoTpCClientOnly(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    UDSTpISOTpC_t *client_isotp = malloc(sizeof(UDSTpISOTpC_t));
    strcpy(client_isotp->tag, "client");
    assert(UDS_OK == UDSTpISOTpCInit(client_isotp, "vcan0", 0x7e0, 0x7e8, 0, 0x7df));
    env->client_tp = (UDSTpHandle_t *)client_isotp;
    *state = env;
    return 0;
}

int TeardownIsoTpCClientOnly(void **state) {
    Env_t *env = *state;
    UDSTpISOTpCDeinit((UDSTpISOTpC_t *)env->client_tp);
    free(env->client_tp);
    free(env);
    return 0;
}

int SetupIsoTpSockPair(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    UDSTpIsoTpSock_t *server_isotp = malloc(sizeof(UDSTpIsoTpSock_t));
    strcpy(server_isotp->tag, "server");
    assert(UDS_OK == UDSTpIsoTpSockInitServer(server_isotp, "vcan0", 0x7e8, 0x7e0, 0x7df));
    env->server_tp = (UDSTpHandle_t *)server_isotp;

    UDSTpIsoTpSock_t *client_isotp = malloc(sizeof(UDSTpIsoTpSock_t));
    strcpy(client_isotp->tag, "client");
    assert(UDS_OK == UDSTpIsoTpSockInitClient(client_isotp, "vcan0", 0x7e0, 0x7e8, 0x7df));
    env->client_tp = (UDSTpHandle_t *)client_isotp;

    *state = env;
    return 0;
}

int TeardownIsoTpSockPair(void **state) {
    Env_t *env = *state;
    UDSTpIsoTpSockDeinit((UDSTpIsoTpSock_t *)env->server_tp);
    UDSTpIsoTpSockDeinit((UDSTpIsoTpSock_t *)env->client_tp);
    free(env->server_tp);
    free(env->client_tp);
    free(env);
    return 0;
}

int SetupIsoTpSockClientOnly(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    UDSTpIsoTpSock_t *client_isotp = malloc(sizeof(UDSTpIsoTpSock_t));
    strcpy(client_isotp->tag, "client");
    assert(UDS_OK == UDSTpIsoTpSockInitClient(client_isotp, "vcan0", 0x7e0, 0x7e8, 0x7df));
    env->client_tp = (UDSTpHandle_t *)client_isotp;
    *state = env;
    return 0;
}

int TeardownIsoTpSockClientOnly(void **state) {
    Env_t *env = *state;
    UDSTpIsoTpSockDeinit((UDSTpIsoTpSock_t *)env->client_tp);
    free(env->client_tp);
    free(env);
    return 0;
}

void test_send_recv(void **state) {
    Env_t *e = *state;

    // When some data is sent by one transport
    const uint8_t MSG[] = {0x10, 0x02};
    UDSTpSend(e->client_tp, MSG, sizeof(MSG), NULL);

    // it should be received soon by the other transport.
    EXPECT_WITHIN_MS(e, UDSTpGetRecvLen(e->server_tp) > 0, 10);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(e->server_tp, NULL), MSG, sizeof(MSG));
}

void test_send_recv_functional(void **state) {
    Env_t *e = *state;

    // When a functional request is sent
    const uint8_t MSG[] = {0x10, 0x02};
    UDSSDU_t info = {0};
    UDSTpSend(e->client_tp, MSG, sizeof(MSG), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});


    // the server should receive it quickly
    EXPECT_WITHIN_MS(e, UDSTpGetRecvLen(e->server_tp) > 0, 10);

    // it should be the same message
    uint8_t *buf = NULL;
    UDSSDU_t info2 = {0};
    UDSTpPeek(e->server_tp, &buf, &info2);
    TEST_MEMORY_EQUAL(buf, MSG, sizeof(MSG));

    // and the server should know it's a functional request
    assert_int_equal(info2.A_TA_Type, UDS_A_TA_TYPE_FUNCTIONAL);
}

void test_send_recv_largest_single_frame(void **state) {
    Env_t *e = *state;

    // When a functional request is sent
    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7};
    UDSSDU_t info = {0};
    UDSTpSend(e->client_tp, MSG, sizeof(MSG), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    // the server should receive it quickly
    EXPECT_WITHIN_MS(e, UDSTpGetRecvLen(e->server_tp) > 0, 10);

    // it should be the same message
    uint8_t *buf = NULL;
    UDSSDU_t info2 = {0};
    UDSTpPeek(e->server_tp, &buf, &info2);
    TEST_MEMORY_EQUAL(buf, MSG, sizeof(MSG));

    // and the server should know it's a functional request
    assert_int_equal(info2.A_TA_Type, UDS_A_TA_TYPE_FUNCTIONAL);
}

// ISO 15765-2 2016 Table 4 note b. Functional addressing shall only be supported for SingleFrame communication.
void test_send_functional_larger_than_single_frame_fails(void **state){
    Env_t *e = *state;

    // When a functional request is sent with more than 7 bytes
    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7, 8};
    ssize_t ret = UDSTpSend(e->client_tp, MSG, sizeof(MSG), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    // it should fail
    assert_true(ret < 0);
}

void test_send_recv_max_len(void **state) {
    Env_t *e = *state;

    // When a request is sent with the maximum length
    uint8_t MSG[4095] = {0};
    MSG[0] = 0x10;
    MSG[4094] = 0x02;
    UDSTpSend(e->client_tp, MSG, sizeof(MSG), NULL);

    // the server should receive it quickly, albeit perhaps with a slight delay on vcan
    EXPECT_WITHIN_MS(e, UDSTpGetRecvLen(e->server_tp) > 0, 1000);

    // it should be the same message
    uint8_t *buf = NULL;
    UDSSDU_t info = {0};
    UDSTpPeek(e->server_tp, &buf, &info);
    TEST_MEMORY_EQUAL(buf, MSG, sizeof(MSG));
}


void test_flow_control_frame_timeout(void **state) {
    Env_t *e = *state;

    // sending multiframe to wait for Flow Control frame
    // which will not arrive since no server is running
    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7, 8};
    ssize_t ret = UDSTpSend(e->client_tp, MSG, sizeof(MSG), NULL);

    EnvRunMillis(e, 1000);

    skip();
    // The transport implemenation is flawed. It needs a way of returning a status asynchronously.

    // failure is expected as the elapsed 1s timeout raises an error on the ISOTP socket
    assert_true(ret < 0);
}

const struct CMUnitTest tests_tp_mock[] = {
    cmocka_unit_test_setup_teardown(test_send_recv, SetupMockTpPair, TeardownMockTpPair),
    cmocka_unit_test_setup_teardown(test_send_recv_functional, SetupMockTpPair,
                                    TeardownMockTpPair),
    cmocka_unit_test_setup_teardown(test_send_recv_largest_single_frame, SetupMockTpPair,
                                    TeardownMockTpPair),
    cmocka_unit_test_setup_teardown(test_send_functional_larger_than_single_frame_fails,
                                    SetupMockTpPair, TeardownMockTpPair),
    cmocka_unit_test_setup_teardown(test_send_recv_max_len, SetupMockTpPair,
                                    TeardownMockTpPair),
    cmocka_unit_test_setup_teardown(test_flow_control_frame_timeout, SetupMockTpClientOnly,
                                    TeardownMockTpClientOnly),
};

const struct CMUnitTest tests_tp_isotp_c[] = {
    cmocka_unit_test_setup_teardown(test_send_recv, SetupIsoTpCPair, TeardownIsoTpCPair),
    cmocka_unit_test_setup_teardown(test_send_recv_functional, SetupIsoTpCPair,
                                    TeardownIsoTpCPair),
    cmocka_unit_test_setup_teardown(test_send_recv_largest_single_frame, SetupIsoTpCPair,
                                    TeardownIsoTpCPair),
    cmocka_unit_test_setup_teardown(test_send_functional_larger_than_single_frame_fails, SetupIsoTpCPair,
                                    TeardownIsoTpCPair),
    cmocka_unit_test_setup_teardown(test_send_recv_max_len, SetupIsoTpCPair,
                                    TeardownIsoTpCPair),
    cmocka_unit_test_setup_teardown(test_flow_control_frame_timeout, SetupIsoTpCClientOnly,
                                    TeardownIsoTpCClientOnly),
};


const struct CMUnitTest tests_tp_isotp_sock[] = {
    cmocka_unit_test_setup_teardown(test_send_recv, SetupIsoTpSockPair, TeardownIsoTpSockPair),
    cmocka_unit_test_setup_teardown(test_send_recv_functional, SetupIsoTpSockPair,
                                    TeardownIsoTpSockPair),
    cmocka_unit_test_setup_teardown(test_send_recv_largest_single_frame, SetupIsoTpSockPair,
                                    TeardownIsoTpSockPair),
    cmocka_unit_test_setup_teardown(test_send_functional_larger_than_single_frame_fails, SetupIsoTpSockPair, TeardownIsoTpSockPair),
    cmocka_unit_test_setup_teardown(test_send_recv_max_len, SetupIsoTpSockPair,
                                    TeardownIsoTpSockPair),
    cmocka_unit_test_setup_teardown(test_flow_control_frame_timeout, SetupIsoTpSockClientOnly, TeardownIsoTpSockClientOnly),
};

int main(int ac, char **av) {
    if (ac > 1) {
        // because these are compliance tests and we explicitly want to run the same test functions
        // against different types of transport, filtering by the test name with
        // cmocka_set_test_filter won't work.
        if (0 == strcmp(av[1], "mock")) {
            UDS_LOGI(__FILE__, "running mock tests. av[1]=%s", av[1]);
            return cmocka_run_group_tests(tests_tp_mock, NULL, NULL);
        } else if (0 == strcmp(av[1], "c")) {
            UDS_LOGI(__FILE__, "running isotp_c tests. av[1]=%s", av[1]);
            return cmocka_run_group_tests(tests_tp_isotp_c, NULL, NULL);
        } else if (0 == strcmp(av[1], "sock")) {
            UDS_LOGI(__FILE__, "running isotp_sock tests. av[1]=%s", av[1]);
            return cmocka_run_group_tests(tests_tp_isotp_sock, NULL, NULL);
        } else {
            UDS_LOGE(__FILE__, "unknown test type: %s", av[1]);
        }
    }
    const struct CMUnitTest all_tests[] = {
        tests_tp_mock,
        tests_tp_isotp_c,
        tests_tp_isotp_sock,
    };
    UDS_LOGI(__FILE__, "running all tests");
    return cmocka_run_group_tests(all_tests, NULL, NULL);
}
