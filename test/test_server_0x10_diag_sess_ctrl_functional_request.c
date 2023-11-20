#include "test/test.h"

int main() {
    UDSTpHandle_t *mock_client = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);

    // When server is sent a diagnostic session control request
    const uint8_t REQ[] = {0x10, 0x02};
    UDSTpSend(mock_client, REQ, sizeof(REQ), &(UDSSDU_t){
        .A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL
    });

    // the server should respond with a negative response within p2 ms
    const uint8_t EXP_RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_IN_APPROX_MS((UDSTpGetRecvLen(mock_client) > 0), srv.p2_ms)
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), EXP_RESP, sizeof(EXP_RESP));
}
