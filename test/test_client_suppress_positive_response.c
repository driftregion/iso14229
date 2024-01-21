#include "test/test.h"

int main() {
    UDSClient_t client;
    ENV_CLIENT_INIT(client);

    // Setting the suppressPositiveResponse flag before sending a request
    client.options |= UDS_SUPPRESS_POS_RESP;
    UDSSendECUReset(&client, kHardReset);

    // and not receiving a response after approximately p2 ms
    ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_MS + 10);

    // should not result in an error.
    TEST_INT_EQUAL(UDS_OK, client.err);
    TEST_INT_EQUAL(kRequestStateIdle, client.state);
}
