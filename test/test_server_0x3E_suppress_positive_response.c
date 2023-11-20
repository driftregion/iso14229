#include "test/test.h"

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    return kPositiveResponse;
}

int main() {
    UDSTpHandle_t *mock_client = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;

    // when the suppressPositiveResponse bit is set
    const uint8_t REQ[] = {0x3E, 0x80};
    UDSTpSend(mock_client,  REQ, sizeof(REQ), NULL);

    // there should be no response
    ENV_RunMillis(5000);
    TEST_INT_EQUAL(UDSTpGetRecvLen(mock_client), 0);
}
