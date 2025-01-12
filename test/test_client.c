#include "test/env.h"

int Setup(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    env->client = malloc(sizeof(UDSClient_t));
    UDSClientInit(env->client);
    env->server_tp = ISOTPMockNew("server", &(ISOTPMockArgs_t){.sa_phys = 0x7E0,
                                                               .ta_phys = 0x7E8,
                                                               .sa_func = 0x7DF,
                                                               .ta_func = UDS_TP_NOOP_ADDR});
    env->client->tp = ISOTPMockNew("client", &(ISOTPMockArgs_t){.sa_phys = 0x7E8,
                                                               .ta_phys = 0x7E0,
                                                               .sa_func = UDS_TP_NOOP_ADDR,
                                                               .ta_func = 0x7DF});
    *state = env;
    return 0;
}

int Teardown(void **state) {
    Env_t *env = *state;
    ISOTPMockFree(env->server_tp);
    ISOTPMockFree(env->client->tp);
    ISOTPMockReset();
    free(env->client);
    free(env);
    return 0;
}

int fn_test_poll_events(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    TEST_INT_EQUAL(evt, UDS_EVT_Poll);
    int *count = (int*)client->fn_data;
    (*count)++;
    return UDS_OK;
}

void test_poll_events(void **state) {
    Env_t *e = *state;
    UDSEvent_t *evt = NULL;
    int count = 0;
    e->client->fn = fn_test_poll_events;
    e->client->fn_data = &count;
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(count, 1000);
}


int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_poll_events, Setup, Teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}