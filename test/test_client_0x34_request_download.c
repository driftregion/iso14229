#include "test/env.h"

int main() {
    UDSClient_t client;
    ENV_CLIENT_INIT(client);

    // When the client sends a request download request
    EXPECT_OK(UDSSendRequestDownload(&client, 0x11, 0x33, 0x602000, 0x00FFFF));

    // the bytes sent should match UDS-1 2013 Table 415
    const uint8_t CORRECT_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    TEST_MEMORY_EQUAL(client.send_buf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));
}