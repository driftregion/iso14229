#include "iso14229.h"
#include "test/test.h"

static uint8_t resp;
static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) { return resp; }

// ISO-14229-1 2013 Table A.1 Byte Value 0x78: requestCorrectlyReceived-ResponsePending
// "This NRC is in general supported by each diagnostic service".
int main() {
    UDSTpHandle_t *mock_client = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;

    // When a server handler func initially returns RRCRP
    resp = kRequestCorrectlyReceived_ResponsePending;

    // sending a request to the server should return RCRRP within p2 ms
    const uint8_t REQUEST[] = {0x31, 0x01, 0x12, 0x34};
    UDSTpSend(mock_client,  REQUEST, sizeof(REQUEST), NULL);

    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78};

    // There should be no response until P2_server has elapsed
    EXPECT_IN_APPROX_MS((UDSTpGetRecvLen(mock_client) > 0), srv.p2_ms)
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RCRRP, sizeof(RCRRP));
    UDSTpAckRecv(mock_client);

    // The server should again respond within p2_star ms
    EXPECT_IN_APPROX_MS((UDSTpGetRecvLen(mock_client) > 0), srv.p2_star_ms * 0.3)
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RCRRP, sizeof(RCRRP));
    UDSTpAckRecv(mock_client);

    // and keep responding at intervals of p2_star ms
    EXPECT_IN_APPROX_MS((UDSTpGetRecvLen(mock_client) > 0), srv.p2_star_ms * 0.3)
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RCRRP, sizeof(RCRRP));
    UDSTpAckRecv(mock_client);

    EXPECT_IN_APPROX_MS((UDSTpGetRecvLen(mock_client) > 0), srv.p2_star_ms * 0.3)
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RCRRP, sizeof(RCRRP));
    UDSTpAckRecv(mock_client);

    // When the user func now returns a positive response
    resp = kPositiveResponse;

    // the server's next response should be a positive one
    // and it should arrive within p2 ms
    const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
    EXPECT_IN_APPROX_MS((UDSTpGetRecvLen(mock_client) > 0), srv.p2_ms)
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE));
}
