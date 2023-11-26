#include "iso14229.h"
#include "test/test.h"

uint8_t fn_callCount = 0;
uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    switch (ev) {
    case UDS_SRV_EVT_EcuReset:
        fn_callCount += 1;
        return kPositiveResponse;
    default:
        TEST_INT_EQUAL(UDS_SRV_EVT_DoScheduledReset, ev);
        return kPositiveResponse;
    }
}

int main() {
    UDSTpHandle_t *mock_client = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;

    const uint8_t REQ[] = {0x11, 0x01};
    const uint8_t RESP[] = {0x51, 0x01};

    // Sending the first ECU reset should result in a response
    UDSTpSend(mock_client, REQ, sizeof(REQ), NULL);
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client), srv.p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RESP, sizeof(RESP));

    UDSTpAckRecv(mock_client);

    // Sending subsequent ECU reset requests should never receive any response
    const unsigned LONG_TIME_MS = 5000;
    UDSTpSend(mock_client, REQ, sizeof(REQ), NULL);
    EXPECT_WHILE_MS(UDSTpGetRecvLen(mock_client) == 0, LONG_TIME_MS);

    // Additionally the ECU reset handler should have been called exactly once.
    TEST_INT_EQUAL(fn_callCount, 1);
}
