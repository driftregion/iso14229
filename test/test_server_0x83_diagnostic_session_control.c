#include "test/env.h"

static uint8_t ReturnPositiveResponse(UDSServer_t *srv, UDSEvent_t ev, const void *arg) {
    return UDS_PositiveResponse;
}

int main() {
    UDSTpHandle_t *mock_client = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = ReturnPositiveResponse;

    // the server sessionType after initialization should be kDefaultSession.
    TEST_INT_EQUAL(srv.sessionType, kDefaultSession);

    // when a request is sent with the suppressPositiveResponse bit set
    const uint8_t REQ[] = {0x10, 0x83};
    UDSTpSend(mock_client, REQ, sizeof(REQ), NULL);

    // even after running for a long time
    ENV_RunMillis(5000);
    // there should be no response from the server
    TEST_INT_EQUAL(UDSTpGetRecvLen(mock_client), 0);

    // however, the server sessionType should have changed
    TEST_INT_EQUAL(srv.sessionType, kExtendedDiagnostic);
}