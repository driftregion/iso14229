#include "test/env.h"


int server_test_setup(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    env->server = malloc(sizeof(UDSServer_t));
    UDSServerInit(env->server);
    env->server->tp = ISOTPMockNew("server", ISOTPMock_DEFAULT_SERVER_ARGS);
    env->client_tp = ISOTPMockNew("client", ISOTPMock_DEFAULT_CLIENT_ARGS);
    *state = env;
    return 0;
}

int server_test_teardown(void **state) {
    Env_t *env = *state;
    ISOTPMockFree(env->server->tp);
    ISOTPMockFree(env->client_tp);
    free(env->server);
    free(env);
    return 0;
}

void test_server_0x10_diag_sess_ctrl_functional_request(void **state) {
    Env_t *e = *state;

    const uint8_t REQ[] = {0x10, 0x02};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    const uint8_t EXP_RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_IN_APPROX_MS(e, (UDSTpGetRecvLen(e->client_tp) > 0), e->server->p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(e->client_tp, NULL), EXP_RESP, sizeof(EXP_RESP));
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_server_0x10_diag_sess_ctrl_functional_request, server_test_setup, server_test_teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
