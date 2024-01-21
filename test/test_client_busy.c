#include "test/test.h"

int main() {
    UDSClient_t client;
    ENV_CLIENT_INIT(client);

    // Sending a request should not return an error
    EXPECT_OK(UDSSendECUReset(&client, kHardReset));

    // unless there is an existing unresolved request
    TEST_INT_EQUAL(UDS_ERR_BUSY, UDSSendECUReset(&client, kHardReset));
}