#include "test/test.h"

int main() {
    UDSSess_t mock_client;
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    ENV_SESS_INIT(mock_client);

    // When server is sent a diagnostic session control request
    const uint8_t REQ[] = {0x10, 0x02};
    EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));

    // the server should respond with a negative response within p2 ms
    const uint8_t EXP_RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_WITHIN_MS((mock_client.recv_size > 0), 50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, EXP_RESP, sizeof(EXP_RESP));
}
