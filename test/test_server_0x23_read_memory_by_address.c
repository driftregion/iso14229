#include "test/test.h"

uint8_t FakeData[259];

uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    TEST_INT_EQUAL(ev, UDS_SRV_EVT_ReadMemByAddr);
    UDSReadMemByAddrArgs_t *r = (UDSReadMemByAddrArgs_t *)arg;
    TEST_PTR_EQUAL(r->memAddr, (void *)0x20481392);
    TEST_INT_EQUAL(r->memSize, 259);
    return r->copy(srv, FakeData, r->memSize);
}

int main() {
    UDSTpHandle_t *mock_client = ENV_GetMockTp("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;
    ENV_SESS_INIT(mock_client);

    // Prepare fake data
    for (int i = 0; i < sizeof(FakeData); i++) {
        FakeData[i] = i % 256;
    }

    // Prepare expected response
    uint8_t EXPECTED_RESP[sizeof(FakeData) + 1];
    EXPECTED_RESP[0] = 0x63; // SID 0x23 + 0x40
    for (int i = 0; i < sizeof(FakeData); i++) {
        EXPECTED_RESP[i + 1] = FakeData[i];
    }

    // Request per ISO14229-1 2020 Table 200
    const uint8_t REQ[] = {
        0x23, // SID
        0x24, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2
        0x13, // memoryAddress byte #3
        0x92, // memoryAddress byte #4 (LSB)
        0x01, // memorySize byte #1 (MSB)
        0x03, // memorySize byte #2 (LSB)
    };
    UDSTpSend(mock_client,  REQ, sizeof(REQ), NULL);

    // the server should respond with a positive response within p2 ms
    EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) > 0, srv.p2_ms);
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// TODO: Tables 202-205