#include "iso14229.h"
#include "test/env.h"

uint8_t FakeData[259];

uint8_t fn(UDSServer_t *srv, UDSEvent_t ev, const void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_ReadMemByAddr);
    UDSReadMemByAddrArgs_t *r = (UDSReadMemByAddrArgs_t *)arg;
    TEST_PTR_EQUAL(r->memAddr, (void *)0x20481392);
    TEST_INT_EQUAL(r->memSize, 259);
    return r->copy(srv, FakeData, r->memSize);
}

int main() {
    UDSTpHandle_t *client_tp = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;

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
    UDSTpSend(client_tp, REQ, sizeof(REQ), NULL);

    // the client transport should receive a positive response within client_p2 ms
    // EXPECT_WITHIN_MS(UDSTpGetRecvLen(client_tp) > 0, UDS_CLIENT_DEFAULT_P2_MS)
    uint32_t deadline = UDSMillis() + UDS_CLIENT_DEFAULT_P2_MS + 1;
    while (!(UDSTpGetRecvLen(client_tp) > 0)) {
        printf("UDSTpGetRecvLen(client_tp) = %ld\n", UDSTpGetRecvLen(client_tp));
        TEST_INT_LE(UDSMillis(), deadline);
        ENV_RunMillis(1);
    }
    TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(client_tp, NULL), EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// TODO: Tables 202-205