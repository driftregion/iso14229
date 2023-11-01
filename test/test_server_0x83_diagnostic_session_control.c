#include "test/test.h"

static uint8_t ReturnPositiveResponse(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    return kPositiveResponse;
}

int main() {
    UDSSess_t mock_client;
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = ReturnPositiveResponse;
    ENV_SESS_INIT(mock_client);

    // the server sessionType after initialization should be kDefaultSession.
    TEST_INT_EQUAL(srv.sessionType, kDefaultSession);

    // when a request is sent with the suppressPositiveResponse bit set
    const uint8_t REQ[] = {0x10, 0x83};
    EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));

    // even after running for a long time
    ENV_RunMillis(5000);
    // there should be no response from the server
    TEST_INT_EQUAL(mock_client.recv_size, 0);

    // however, the server sessionType should have changed
    TEST_INT_EQUAL(srv.sessionType, kExtendedDiagnostic);
}