#include "test/test.h"

int main() {
    UDSClient_t client;
    ENV_CLIENT_INIT(client);

    // attempting to send a request payload of 6 bytes
    uint16_t didList[] = {0x0001, 0x0002, 0x0003};

    // which is larger than the underlying buffer
    client.send_buf_size = 4;

    // should return an error
    TEST_INT_EQUAL(UDS_ERR_INVALID_ARG,
                   UDSSendRDBI(&client, didList, sizeof(didList) / sizeof(didList[0])))

    // and no data should be sent
    TEST_INT_EQUAL(client.send_size, 0);
}