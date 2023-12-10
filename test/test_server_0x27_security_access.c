#include "test/test.h"

static UDSTpHandle_t *mock_client = NULL;
static UDSServer_t srv;

// UDS-1 2013 9.4.5.2
// UDS-1 2013 9.4.5.3
uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    switch (ev) {
    case UDS_SRV_EVT_SecAccessRequestSeed: {
        UDSSecAccessRequestSeedArgs_t *r = (UDSSecAccessRequestSeedArgs_t *)arg;
        const uint8_t seed[] = {0x36, 0x57};
        TEST_INT_NE(r->level, srv->securityLevel);
        return r->copySeed(srv, seed, sizeof(seed));
        break;
    }
    case UDS_SRV_EVT_SecAccessValidateKey: {
        UDSSecAccessValidateKeyArgs_t *r = (UDSSecAccessValidateKeyArgs_t *)arg;
        const uint8_t expected_key[] = {0xC9, 0xA9};
        if (memcmp(r->key, expected_key, sizeof(expected_key))) {
            return kSecurityAccessDenied;
        } else {
            return kPositiveResponse;
        }
        break;
    }
    default:
        assert(0);
    }
    return kPositiveResponse;
}

int setup(void **state) {
    mock_client = ENV_TpNew("client");
    ENV_SERVER_INIT(srv);
    srv.fn = fn;
    return 0;
}

int teardown(void **state) {
    ENV_TpFree(mock_client);
    ENV_TpFree(srv.tp);
    return 0;
}

void TestSAInit(void **state) {
    // the server security level after initialization should be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);
}


void TestSABruteForcePrevention1(void **state) {
    // sending a seed request should get this response
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    const uint8_t NEG_RESPONSE[] = {0x7F, 0x27, 0x37};
    UDSTpSend(mock_client, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) != 0, srv.p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), NEG_RESPONSE, sizeof(NEG_RESPONSE));
    UDSTpAckRecv(mock_client);

    // the server security level should still be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);
}

void TestSABruteForcePrevention2(void **state) {
    // the server security level after initialization should be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);

    // Wait for the anti-brute-force timeout to expire
    ENV_RunMillis(1000);

    // sending a seed request should get this response
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    const uint8_t SEED_RESPONSE[] = {0x67, 0x01, 0x36, 0x57};
    UDSTpSend(mock_client, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);
    EXPECT_WITHIN_MS(UDSTpGetRecvLen(mock_client) != 0, UDS_SERVER_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), SEED_RESPONSE, sizeof(SEED_RESPONSE));
    UDSTpAckRecv(mock_client);

    // the server security level should still be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);

    // subsequently sending an unlock request should get this response
    const uint8_t BAD_UNLOCK_REQUEST[] = {0x27, 0x02, 0xFF, 0xFF};
    const uint8_t UNLOCK_FAIL[] = {0x7F, 0x27, 0x33};
    UDSTpSend(mock_client, BAD_UNLOCK_REQUEST, sizeof(BAD_UNLOCK_REQUEST), NULL);
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) != 0, srv.p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), UNLOCK_FAIL, sizeof(UNLOCK_FAIL));
    UDSTpAckRecv(mock_client);

    // the server security level should still be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);

    // sending another seed request should get denied
    const uint8_t DENIED[] = {0x7F, 0x27, 0x36};
    UDSTpSend(mock_client, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);
    EXPECT_WITHIN_MS(UDSTpGetRecvLen(mock_client) != 0, UDS_SERVER_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), DENIED, sizeof(DENIED));
    UDSTpAckRecv(mock_client);
}

void TestSAUnlock(void **state) {
    // Wait for the anti-brute-force timeout to expire
    ENV_RunMillis(1000);

    // sending a seed request should get this response
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    const uint8_t SEED_RESPONSE[] = {0x67, 0x01, 0x36, 0x57};
    UDSTpSend(mock_client, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);
    EXPECT_WITHIN_MS(UDSTpGetRecvLen(mock_client) != 0, UDS_SERVER_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), SEED_RESPONSE, sizeof(SEED_RESPONSE));
    UDSTpAckRecv(mock_client);

    // the server security level should still be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);

    // subsequently sending an unlock request should get this response
    const uint8_t UNLOCK_REQUEST[] = {0x27, 0x02, 0xC9, 0xA9};
    const uint8_t UNLOCK_RESPONSE[] = {0x67, 0x02};
    UDSTpSend(mock_client, UNLOCK_REQUEST, sizeof(UNLOCK_REQUEST), NULL);
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) != 0, srv.p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), UNLOCK_RESPONSE, sizeof(UNLOCK_RESPONSE));
    UDSTpAckRecv(mock_client);

    // the server security level should now be 1
    TEST_INT_EQUAL(srv.securityLevel, 1);

    // sending the same seed request should now result in the "already unlocked" response
    const uint8_t ALREADY_UNLOCKED_RESPONSE[] = {0x67, 0x01, 0x00, 0x00};
    UDSTpSend(mock_client, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) != 0, srv.p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), ALREADY_UNLOCKED_RESPONSE,
                      sizeof(ALREADY_UNLOCKED_RESPONSE));

    // Additionally, the security level should still be 1
    TEST_INT_EQUAL(srv.securityLevel, 1);
}


int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(TestSAInit, setup, teardown),
        cmocka_unit_test_setup_teardown(TestSABruteForcePrevention1, setup, teardown),
        cmocka_unit_test_setup_teardown(TestSABruteForcePrevention2, setup, teardown),
        cmocka_unit_test_setup_teardown(TestSAUnlock, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}


