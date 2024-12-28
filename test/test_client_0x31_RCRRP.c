#include "iso14229.h"
#include "test/env.h"

static UDSClient_t client;
static UDSTpHandle_t *mock_srv;


int setup(void **state) {
    ENV_CLIENT_INIT(client)
    mock_srv = ENV_TpNew("server");
    return 0;
}

int teardown(void **state) {
    ENV_TpFree(mock_srv);
    ENV_TpFree(client.tp);
    return 0;
}

// This is too complicated. State what this test is doing and find a way to make it simpler
// This test is:
// - sending a 0x31 Routine Control request with UDSSendRoutineCtrl(...)
// - mocking a server "Response Pending" response
// - asserting that the client does not time out until after UDS_CLIENT_DEFAULT_P2_STAR_MS has elapsed

static int fn_test_RCRRP_timeout(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    int *step = (int *)client->fn_data;
    switch (*step) {
        case 0:
            switch (evt) {
                case UDS_EVT_Idle:
                    // When a request is sent
                    UDSSendRoutineCtrl(client, kStartRoutine, 0x1234, NULL, 0);

                    // that receives an RCRRP response
                    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
                    UDSTpSend(mock_srv, RCRRP, sizeof(RCRRP), NULL);

                    (*step)++;
                    break;
            }
        case 1:{
            if (UDSMillis() < UDS_CLIENT_DEFAULT_P2_MS)
            switch (evt) {
                case UDS_EVT_Err: {
                    UDSErr_t err = *(UDSErr_t*)ev_data;
                    assert_int_equal(err, UDS_ERR_TIMEOUT);
                    TEST_INT_GREATER(UDSMillis(), UDS_CLIENT_DEFAULT_P2_STAR_MS);
                }
            }
        }
    }
    return UDS_OK;
}


static int fn_assert_err_is(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    if (evt == UDS_EVT_Err){
        UDSErr_t err = *(UDSErr_t*)ev_data;
        UDSErr_t wanted = *(UDSErr_t*)client->fn_data;
        assert_int_equal(err, UDS_OK);
    }
    return UDS_OK;
}

static int fn_assert_no_err(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    assert_int_not_equal(evt, UDS_EVT_Err);
    return UDS_OK;
}

void test_RCRRP_timeout(void **state) {
    // client.fn = fn_test_RCRRP_timeout;
    // int step = 0;
    // client.fn_data = &step;

    UDSErr_t err = UDSSendRoutineCtrl(&client, kStartRoutine, 0x1234, NULL, 0);
    TEST_ERR_EQUAL(UDS_OK, err);

    // that receives an RCRRP response
    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
    UDSTpSend(mock_srv, RCRRP, sizeof(RCRRP), NULL);

    client.fn = fn_assert_no_err;

    // that remains unresolved at a time between p2 ms and p2 star ms
    ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_MS + 10);

    // after p2_star has elapsed, the client should timeout
    ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_STAR_MS + 10);
    // TEST_INT_EQUAL(err, UDS_ERR_TIMEOUT);
    ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_STAR_MS + 10);

    err = UDS_ERR_TIMEOUT;
    err = UDS_OK;
    client.fn_data = &err;
}

// void test_RCRRP_positive_response(void **state) {
//     // When a request is sent
//     UDSSendRoutineCtrl(&client, kStartRoutine, 0x1234, NULL, 0);

//     // that receives an RCRRP response
//     const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
//     UDSTpSend(mock_srv, RCRRP, sizeof(RCRRP), NULL);

//     // that remains unresolved at a time between p2 ms and p2 star ms
//     ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_MS + 10);
//     // the client should still be pending.
//     TEST_INT_EQUAL(err, UDS_OK);
//     TEST_INT_EQUAL(kRequestStateAwaitResponse, client.state)

//     // When the client receives a positive response from the server
//     const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
//     UDSTpSend(mock_srv, POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE), NULL);

//     ENV_RunMillis(5);

//     // the client should return to the idle state with no error
//     TEST_INT_EQUAL(kRequestStateIdle, client.state)
//     TEST_INT_EQUAL(err, UDS_OK);
// }

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_RCRRP_timeout, setup, teardown),
        // cmocka_unit_test_setup_teardown(test_RCRRP_positive_response, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}