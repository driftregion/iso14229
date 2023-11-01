#include "test/test.h"

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
        TEST_INT_EQUAL(r->len, sizeof(expected_key));
        TEST_MEMORY_EQUAL(r->key, expected_key, sizeof(expected_key));
        break;
    }
    default:
        assert(0);
    }
    return kPositiveResponse;
}

int main() {
    UDSSess_t mock_client;
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;
    ENV_SESS_INIT(mock_client);

    // the server security level after initialization should be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);

    // sending a seed request should get this response
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    const uint8_t SEED_RESPONSE[] = {0x67, 0x01, 0x36, 0x57};
    EXPECT_OK(UDSSessSend(&mock_client, SEED_REQUEST, sizeof(SEED_REQUEST)));
    ENV_RunMillis(50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, SEED_RESPONSE, sizeof(SEED_RESPONSE));

    // the server security level should still be 0
    TEST_INT_EQUAL(srv.securityLevel, 0);

    // subsequently sending an unlock request should get this response
    const uint8_t UNLOCK_REQUEST[] = {0x27, 0x02, 0xC9, 0xA9};
    const uint8_t UNLOCK_RESPONSE[] = {0x67, 0x02};
    EXPECT_OK(UDSSessSend(&mock_client, UNLOCK_REQUEST, sizeof(UNLOCK_REQUEST)));
    ENV_RunMillis(50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, UNLOCK_RESPONSE, sizeof(UNLOCK_RESPONSE));

    // the server security level should now be 1
    TEST_INT_EQUAL(srv.securityLevel, 1);

    // sending the same seed request should now result in the "already unlocked" response
    const uint8_t ALREADY_UNLOCKED_RESPONSE[] = {0x67, 0x01, 0x00, 0x00};
    EXPECT_OK(UDSSessSend(&mock_client, SEED_REQUEST, sizeof(SEED_REQUEST)));
    ENV_RunMillis(50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, ALREADY_UNLOCKED_RESPONSE,
                      sizeof(ALREADY_UNLOCKED_RESPONSE));

    // Additionally, the security level should still be 1
    TEST_INT_EQUAL(srv.securityLevel, 1);
}