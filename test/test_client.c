#include "test/env.h"


int Setup(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    env->client = malloc(sizeof(UDSClient_t));
    UDSClientInit(env->client);
    env->client->tp = ISOTPMockNew("client", &(ISOTPMockArgs_t){.sa_phys = 0x7E8,
                                                               .ta_phys = 0x7E0,
                                                               .sa_func = UDS_TP_NOOP_ADDR,
                                                               .ta_func = 0x7DF});

    env->mock_server = MockServerNew();
    env->mock_server->tp = ISOTPMockNew("server", &(ISOTPMockArgs_t){.sa_phys = 0x7E0,
                                                               .ta_phys = 0x7E8,
                                                               .sa_func = 0x7DF,
                                                               .ta_func = UDS_TP_NOOP_ADDR});
    *state = env;
    return 0;
}

int Teardown(void **state) {
    Env_t *env = *state;
    ISOTPMockFree(env->client->tp);
    ISOTPMockFree(env->mock_server->tp);
    ISOTPMockReset();
    free(env->client);
    MockServerFree(env->mock_server);
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
    int count = 0;
    e->client->fn = fn_test_poll_events;
    e->client->fn_data = &count;
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(count, 1000);
}

void test_duplicate_request(void **state) {
    Env_t *e = *state;

    // sending a request should succeed
    UDSErr_t err = UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    TEST_ERR_EQUAL(UDS_OK, err);

    // immediately sending another request should fail
    err = UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    TEST_INT_EQUAL(UDS_ERR_BUSY, err);
}


int fn_log_call_count(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    int *call_count = (int*)client->fn_data;
    call_count[evt]++;
    switch (evt) {
        case UDS_EVT_Poll:
            break;
        case UDS_EVT_Err:{
            UDSErr_t err = *(UDSErr_t*)ev_data;
            UDS_LOGI(__FILE__, "error: %s (%d)", UDSErrToStr(err), err);
            break;
        }
        default:
            UDS_LOGI(__FILE__, "client event %s (%d)", UDSEventToStr(evt), evt);
    }
    return UDS_OK;
}

void test_0x11_good_response(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11,0x01}, 
            .req_len = 2, 
            .resp_data = {0x51,0x01},
            .resp_len = 2,
            .delay_ms = 0,
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(call_count[UDS_EVT_ResponseReceived], 1);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 0);
}

void test_0x11_timeout(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11, 0x01},
            .req_len = 2,
            .resp_data = {0x51, 0x01},
            .resp_len = 2,
            .delay_ms = e->client->p2_ms + 10,  // timeout
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 1);
}

void test_0x11_sid_mismatch(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11, 0x01},
            .req_len = 2,
            .resp_data = {0x50, 0x01}, // (bad SID) should be 0x51
            .resp_len = 2,
            .delay_ms = 0, 
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 1);
}

void test_0x11_short_response(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11, 0x01},
            .req_len = 2,
            .resp_data = {0x51}, // short response (ECUReset response should be 2-3 bytes)
            .resp_len = 1,
            .delay_ms = 0,
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 1);
}

void test_0x11_inconsistent_subfunc(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11, 0x01},
            .req_len = 2,
            .resp_data = {0x51, 0x02}, // subfunction (0x02) inconsistent with request (0x01)
            .resp_len = 2,
            .delay_ms = 0, 
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 1);
}

void test_0x11_neg_resp(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11, 0x01},
            .req_len = 2,
            .resp_data = {0x7F, 0x11, 0x10},
            .resp_len = 3,
            .delay_ms = 0,
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 1);
}

void test_0x11_rcrrp_timeout(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11, 0x01},
            .req_len = 2,
            .resp_data = {0x7F, 0x11, 0x78}, // RCRRP
            .resp_len = 3,
            .delay_ms = 0, 
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);

    EnvRunMillis(e, e->client->p2_star_ms - 10);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 0);
    EnvRunMillis(e, 20);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 1);
}

void test_0x11_rcrrp_ok(void **state) {
    Env_t *e = *state;
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x11, 0x01},
            .req_len = 2,
            .resp_data = {0x7F, 0x11, 0x78}, // RCRRP
            .resp_len = 3,
            .delay_ms = 0,
        }
    });
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);

    EnvRunMillis(e, e->client->p2_star_ms - 10);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 0);
    const uint8_t POSITIVE_RESPONSE[] = {0x51, 0x01};
    UDSTpSend(e->mock_server->tp, POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE), NULL);
    EnvRunMillis(e, 20);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 0);
    TEST_INT_EQUAL(call_count[UDS_EVT_ResponseReceived], 1);
}

void test_0x11_suppress_pos_resp(void **state) {
    Env_t *e = *state;
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;

    // when the suppressPositiveResponse flag is set
    e->client->options |= UDS_SUPPRESS_POS_RESP;
    UDSSendECUReset(e->client, UDS_LEV_RT_HR);
    EnvRunMillis(e, 1000);

    // Sending will succeed but there should be no response
    TEST_INT_EQUAL(call_count[UDS_EVT_SendComplete], 1);
    TEST_INT_EQUAL(call_count[UDS_EVT_ResponseReceived], 0);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 0);
}

void test_0x22_unpack_rdbi_response(void **state) {
    Env_t *e = *state;
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;

    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = ExactRequestResponse,
        .exact_request_response = {
            .req_data = {0x22, 0xf1, 0x90},
            .req_len = 3,
            .resp_data = {0x62, 0xf1, 0x90, 0x0a, 0x00},
            .resp_len = 5,
            .delay_ms = 0,
        }
    });

    const uint16_t did_list[] = {0xf190};

    UDSSendRDBI(e->client, did_list, 1);

    EnvRunMillis(e, 1000);

    // and a variable is set up to receive the data
    uint16_t var = 0;
    UDSRDBIVar_t vars[] = {
        {0xf190, 2, &var, memmove}
    };

    UDSErr_t err = UDSUnpackRDBIResponse(e->client, vars, 1);
    TEST_ERR_EQUAL(UDS_OK, err);
    TEST_INT_EQUAL(var, 0x0a);
}

void test_0x34_format(void **state) {
    Env_t *e = *state;
    UDSErr_t err = UDSSendRequestDownload(e->client, 0x11, 0x33, 0x602000, 0x00FFFF);
    TEST_ERR_EQUAL(UDS_OK, err);

    // the bytes sent should match UDS-1 2013 Table 415
    const uint8_t CORRECT_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    TEST_MEMORY_EQUAL(e->client->send_buf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));
}

void test_0x38_format_add_file(void **state) {
    Env_t *e = *state;
    UDSErr_t err = UDSSendRequestFileTransfer(e->client, 0x01, "/data/testfile.zip", 0x00, 3, 0x112233, 0x001122);
    TEST_ERR_EQUAL(UDS_OK, err);

    const uint8_t CORRECT_REQUEST[] = {0x38, 0x01, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x2F, 0x74, 0x65,
                                        0x73, 0x74, 0x66, 0x69, 0x6C, 0x65, 0x2E, 0x7A, 0x69, 0x70, 0x00, 0x03, 
                                        0x11, 0x22, 0x33, 0x00, 0x11, 0x22};
    TEST_MEMORY_EQUAL(e->client->send_buf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));
}

void test_0x38_format_delete_file(void **state) {
    Env_t *e = *state;
    UDSErr_t err = UDSSendRequestFileTransfer(e->client, 0x02, "/data/testfile.zip", 0x00, 0, 0, 0);
    TEST_ERR_EQUAL(UDS_OK, err);

    const uint8_t CORRECT_REQUEST[] = {0x38, 0x02, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x2F, 0x74, 0x65,
                                        0x73, 0x74, 0x66, 0x69, 0x6C, 0x65, 0x2E, 0x7A, 0x69, 0x70};
    TEST_MEMORY_EQUAL(e->client->send_buf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));
}

void test_0x2e_issue_59(void **state) {
    Env_t *e = *state;
    int call_count[UDS_EVT_MAX] = {0};
    e->client->fn = fn_log_call_count;
    e->client->fn_data = call_count;

    // When a server sends an unsolicited but valid response
    MockServerAddBehavior(e->mock_server, &(struct Behavior){
        .tag = TimedSend,
        .timed_send = {
            .send_data = {0x6E, 0x12, 0x34},
            .send_len = 3,
            .when = UDSMillis(),
        }
    });

    EnvRunMillis(e, 50);

    // And the client *later* sends the request which would result in such a response
    uint8_t wdbi_data[] = {0x01, 0x02, 0x03, 0x04};
    UDSErr_t err = UDSSendWDBI(e->client, 0x1234, wdbi_data, sizeof(wdbi_data));
    
    // there request should be sent successfully
    TEST_ERR_EQUAL(UDS_OK, err);

    // but it should later time out
    EnvRunMillis(e, 1000);
    TEST_INT_EQUAL(call_count[UDS_EVT_SendComplete], 1);
    TEST_INT_EQUAL(call_count[UDS_EVT_ResponseReceived], 0);
    TEST_INT_EQUAL(call_count[UDS_EVT_Err], 1);
}

int main(int ac, char **av) {
    if (ac > 1) {
        cmocka_set_test_filter(av[1]);
    }
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_poll_events, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_duplicate_request, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_good_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_timeout, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_sid_mismatch, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_short_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_inconsistent_subfunc, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_neg_resp, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_rcrrp_timeout, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_rcrrp_ok, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_suppress_pos_resp, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x22_unpack_rdbi_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x34_format, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x38_format_add_file, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x38_format_delete_file, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2e_issue_59, Setup, Teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}