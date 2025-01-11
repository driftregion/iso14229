#include "test/env.h"

int fn(UDSServer_t *srv, UDSEvent_t event, void *arg) {
    return UDS_OK;
}

int server_test_setup(void **state) {
    ENV_Context_t *ctx = malloc(sizeof(ENV_Context_t));
    memset(ctx, 0, sizeof(ENV_Context_t));
    ctx->server = malloc(sizeof(UDSServer_t));
    UDSServerInit(ctx->server);
    ctx->server->fn = fn;
    ctx->server->tp = ISOTPMockNew("server", ISOTPMock_DEFAULT_SERVER_ARGS);
    ctx->client_tp = ISOTPMockNew("client", ISOTPMock_DEFAULT_CLIENT_ARGS);
    *state = ctx;
    return 0;
}

int server_test_teardown(void **state) {
    ENV_Context_t *ctx = *state;
    ISOTPMockFree(ctx->server->tp);
    ISOTPMockFree(ctx->client_tp);
    free(ctx->server);
    free(ctx);
    return 0;
}

void test_server_0x10_diag_sess_ctrl_functional_request(void **state) {
    ENV_Context_t *c = *state;
    UDS_LOGI(__func__, "Testing server 0x10 diag sess ctrl functional request");

    const uint8_t REQ[] = {0x10, 0x02};
    UDSTpSend(c->client_tp, REQ, sizeof(REQ), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    const uint8_t EXP_RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_IN_APPROX_MS_C(c, (UDSTpGetRecvLen(c->client_tp) > 0), c->server->p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(c->client_tp, NULL), EXP_RESP, sizeof(EXP_RESP));
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_server_0x10_diag_sess_ctrl_functional_request, server_test_setup, server_test_teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
