#include "iso14229.h"
#include "test.h"

uint8_t fn(UDSServer_t *srv, UDSEvent_t ev, const void *arg) {
    return UDS_PositiveResponse;
}

int main() {
    UDSTpHandle_t *mock_client = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;

    // When RDBI is called on a server that returns data of length zero
    uint8_t REQ[] = {0x22, 0xF1, 0x90};
    UDSTpSend(mock_client, REQ, sizeof(REQ), NULL);
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) > 0, srv.p2_ms)
    uint8_t RESP[] = {0x7F, 0x22, 0x10};
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RESP, sizeof(RESP));
}
