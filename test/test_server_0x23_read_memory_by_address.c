#include "test/test.h"

uint8_t fn10(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    printf("foo!\n");
    TEST_INT_EQUAL(ev, UDS_SRV_EVT_ReadMemByAddr);
    UDSReadMemByAddrArgs_t *r = (UDSReadMemByAddrArgs_t *)arg;
    //                                      1 2 3 4 5 6 7 8
    TEST_PTR_EQUAL(r->memAddr, (void *)0x000055555555f0c8);
    TEST_INT_EQUAL(r->memSize, 4);
    uint8_t FakeData[4] = {0x01, 0x02, 0x03, 0x04};
    return r->copy(srv, FakeData, r->memSize);
}

int main() {
    UDSSess_t mock_client;
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn10;
    ENV_SESS_INIT(mock_client);

    // When the client sends a request download request
    const uint8_t REQ[] = {0x23, 0x18, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0xf0, 0xc8, 0x04};
    EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));

    // the server should respond with a positive response within p2 ms
    uint8_t RESP[] = {0x63, 0x01, 0x02, 0x03, 0x04};
    EXPECT_WITHIN_MS((mock_client.recv_size > 0), 50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, RESP, sizeof(RESP));
}