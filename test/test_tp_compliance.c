#include "test/env.h"
#include "test/test.h"

UDSTpHandle_t *srv = NULL;
UDSTpHandle_t *client = NULL;

static bool IsISOTP() {
    int tp_type = ENV_GetOpts()->tp_type;
    return (ENV_TP_TYPE_ISOTP_SOCK == tp_type) || (ENV_TP_TYPE_ISOTPC == tp_type);
}

int setup(void **state) {
    srv = ENV_TpNew("server");
    client = ENV_TpNew("client");
    TPMockLogToStdout();
    return 0;
}

int teardown(void **state) {
    ENV_TpFree(srv);
    ENV_TpFree(client);
    return 0;
}

void TestSendRecv(void **state) {
    const uint8_t MSG[] = {0x10, 0x02};
    UDSTpSend(client, MSG, sizeof(MSG), NULL);
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(srv) == sizeof(MSG), 1);
    assert_memory_equal(UDSTpGetRecvBuf(srv, NULL), MSG, sizeof(MSG));
}


void TestSendRecvFunctional(void **state) {
    const uint8_t MSG[] = {0x10, 0x02};
    UDSSDU_t info = {0};
    uint8_t *buf = NULL;

    // When a small functional request is sent
    UDSTpSend(client, MSG, sizeof(MSG), &(UDSSDU_t){
        .A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL
    });

    // the server should receive it quickly
    EXPECT_IN_APPROX_MS(UDSTpPeek(srv, &buf, &info) == sizeof(MSG), 1);
    
    // it should be the same message
    assert_memory_equal(buf, MSG, sizeof(MSG));

    // and the server should know it's a functional request
    assert_int_equal(info.A_TA_Type, UDS_A_TA_TYPE_FUNCTIONAL);
}

void TestISOTPSendLargestSingleFrame(void **state) {
    if (!IsISOTP()) {
        skip();
    }
    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7};
    UDSSDU_t info = {0};
    uint8_t *buf = NULL;

    // When a functional request is sent
    ssize_t ret = UDSTpSend(client, MSG, sizeof(MSG), &(UDSSDU_t){
        .A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL
    });

    // the server should receive it quickly
    EXPECT_IN_APPROX_MS(UDSTpPeek(srv, &buf, &info) == sizeof(MSG), 1);
    
    // it should be the same message
    assert_memory_equal(buf, MSG, sizeof(MSG));

    // and the server should know it's a functional request
    assert_int_equal(info.A_TA_Type, UDS_A_TA_TYPE_FUNCTIONAL);
}

void TestISOTPSendLargerThanSingleFrameFails(void **state) {
    if (!IsISOTP()) {
        skip();
    }
    const uint8_t MSG[] = {1, 2, 3, 4, 5, 6, 7, 8};
    UDSSDU_t info = {0};
    uint8_t *buf = NULL;

    // When a small functional request is sent
    ssize_t ret = UDSTpSend(client, MSG, sizeof(MSG), &(UDSSDU_t){
        .A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL
    });
    assert_true(ret < 0);
}

void TestISOTPSendRecvMaxLen(void **state) {
    if (!IsISOTP()) {
        skip();
    }
    uint8_t MSG[4095] = {0};
    MSG[0] = 0x10;
    MSG[4094] = 0x02;

    UDSTpSend(client, MSG, sizeof(MSG), NULL);
    EXPECT_WITHIN_MS(UDSTpGetRecvLen(srv) == sizeof(MSG), 3000);
    assert_memory_equal(UDSTpGetRecvBuf(srv, NULL), MSG, sizeof(MSG));
}

int main() {
    const struct CMUnitTest tests[] = {
        // cmocka_unit_test_setup_teardown(TestSendRecv, setup, teardown),
        cmocka_unit_test_setup_teardown(TestSendRecvFunctional, setup, teardown),
        cmocka_unit_test_setup_teardown(TestISOTPSendLargestSingleFrame, setup, teardown),
        cmocka_unit_test_setup_teardown(TestISOTPSendLargerThanSingleFrameFails, setup, teardown),
        cmocka_unit_test_setup_teardown(TestISOTPSendRecvMaxLen, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}


