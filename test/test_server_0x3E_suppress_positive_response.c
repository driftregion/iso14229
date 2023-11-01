#include "test/test.h"

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    return kPositiveResponse;
}

int main() {
    UDSSess_t mock_client;
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;
    ENV_SESS_INIT(mock_client);

    // when the suppressPositiveResponse bit is set
    const uint8_t REQ[] = {0x3E, 0x80};
    EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));

    // there should be no response
    ENV_RunMillis(5000);
    TEST_INT_EQUAL(mock_client.recv_size, 0);
}
