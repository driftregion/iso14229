#include "test/test.h"

static uint8_t resp;
static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) { return resp; }

// ISO-14229-1 2013 Table A.1 Byte Value 0x78: requestCorrectlyReceived-ResponsePending
// "This NRC is in general supported by each diagnostic service".
int main() {
    UDSSess_t mock_client;
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;
    ENV_SESS_INIT(mock_client);

    // When a server handler func initially returns RRCRP
    resp = kRequestCorrectlyReceived_ResponsePending;

    // sending a request to the server should return RCRRP within p2 ms
    const uint8_t REQUEST[] = {0x31, 0x01, 0x12, 0x34};
    UDSSessSend(&mock_client, REQUEST, sizeof(REQUEST));

    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78};
    EXPECT_WITHIN_MS((mock_client.recv_size > 0), 50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, RCRRP, sizeof(RCRRP));

    // The server should again respond within p2_star ms, and keep responding
    ENV_RunMillis(50);
    EXPECT_WITHIN_MS((mock_client.recv_size > 0), 50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, RCRRP, sizeof(RCRRP));

    ENV_RunMillis(50);
    EXPECT_WITHIN_MS((mock_client.recv_size > 0), 50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, RCRRP, sizeof(RCRRP));

    // When the user func now returns a positive response
    resp = kPositiveResponse;
    ENV_RunMillis(50);

    // the server's next response should be a positive one
    const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
    EXPECT_WITHIN_MS((mock_client.recv_size > 0), 50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE));
}
