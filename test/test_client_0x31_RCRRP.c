#include "test/test.h"

int main() {
    UDSClient_t client;
    UDSTpHandle_t *mock_srv = ENV_GetMockTp("server");
    ENV_SESS_INIT(sess)

    { // Case 1: RCRRP Timeout
        ENV_CLIENT_INIT(client);
        // When a request is sent
        UDSSendRoutineCtrl(&client, kStartRoutine, 0x1234, NULL, 0);

        // that receives an RCRRP response
        const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
        UDSTpSend(mock_srv,  RCRRP, sizeof(RCRRP), NULL);

        // that remains unresolved at a time between p2 ms and p2 star ms
        ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_MS + 10);
        // the client should still be pending.
        TEST_INT_EQUAL(kRequestStateAwaitResponse, client.state)

        // after p2_star has elapsed, the client should timeout
        ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_STAR_MS + 10);
        TEST_INT_EQUAL(kRequestStateIdle, client.state)
        TEST_INT_EQUAL(client.err, UDS_ERR_TIMEOUT);
    }

    { // Case 2: Positive Response Received
        ENV_CLIENT_INIT(client);
        // When a request is sent
        UDSSendRoutineCtrl(&client, kStartRoutine, 0x1234, NULL, 0);

        // that receives an RCRRP response
        const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
        UDSTpSend(mock_srv,  RCRRP, sizeof(RCRRP), NULL);

        // that remains unresolved at a time between p2 ms and p2 star ms
        ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_MS + 10);
        // the client should still be pending.
        TEST_INT_EQUAL(client.err, UDS_OK);
        TEST_INT_EQUAL(kRequestStateAwaitResponse, client.state)

        // When the client receives a positive response from the server
        const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
        UDSTpSend(mock_srv,  POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE), NULL);

        ENV_RunMillis(5);

        // the client should return to the idle state with no error
        TEST_INT_EQUAL(kRequestStateIdle, client.state)
        TEST_INT_EQUAL(client.err, UDS_OK);
    }
}