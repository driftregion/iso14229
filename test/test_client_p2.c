#include "iso14229.h"
#include "test/test.h"

int main() {
    UDSClient_t client;
    UDSTpHandle_t *tp = ENV_GetMockTp("server");

    { // Case 1: P2 not exceeded
        ENV_CLIENT_INIT(client);

        // When the client sends a request
        EXPECT_OK(UDSSendECUReset(&client, kHardReset));

        // which receives a positive response
        const uint8_t POSITIVE_RESPONSE[] = {0x51, 0x01};
        UDSTpSend(tp, POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE), NULL);
        ENV_RunMillis(20);

        // after p2 ms has elapsed, the client should have a timeout error
        TEST_INT_EQUAL(UDS_OK, client.err);
    }
    { // Case 2: P2 exceeded
        ENV_CLIENT_INIT(client);

        // When the client sends a request
        EXPECT_OK(UDSSendECUReset(&client, kHardReset));

        ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_MS - 10);
        // before p2 ms has elapsed, the client should have no error
        TEST_INT_EQUAL(UDS_OK, client.err);

        ENV_RunMillis(20);
        TEST_INT_EQUAL(UDS_ERR_TIMEOUT, client.err);
    }
}