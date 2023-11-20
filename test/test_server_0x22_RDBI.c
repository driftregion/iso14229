#include "iso14229.h"
#include "test/test.h"

uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    TEST_INT_EQUAL(UDS_SRV_EVT_ReadDataByIdent, ev);
    const uint8_t vin[] = {0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30, 0x34, 0x33,
                           0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
    const uint8_t data_0x010A[] = {0xA6, 0x66, 0x07, 0x50, 0x20, 0x1A,
                                   0x00, 0x63, 0x4A, 0x82, 0x7E};
    const uint8_t data_0x0110[] = {0x8C};

    UDSRDBIArgs_t *r = (UDSRDBIArgs_t *)arg;
    switch (r->dataId) {
    case 0xF190:
        return r->copy(srv, vin, sizeof(vin));
    case 0x010A:
        return r->copy(srv, data_0x010A, sizeof(data_0x010A));
    case 0x0110:
        return r->copy(srv, data_0x0110, sizeof(data_0x0110));
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}

int main() {
    UDSTpHandle_t *mock_client = ENV_TpNew("client");
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    srv.fn = fn;
    { // 11.2.5.2 Example #1 read single dataIdentifier 0xF190
        uint8_t REQ[] = {0x22, 0xF1, 0x90};
        UDSTpSend(mock_client,  REQ, sizeof(REQ), NULL);
        uint8_t RESP[] = {0x62, 0xF1, 0x90, 0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30,
                          0x34, 0x33, 0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
        EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) == sizeof(RESP), srv.p2_ms)
        TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RESP, sizeof(RESP));
       UDSTpAckRecv(mock_client);
    }
    { // Read a nonexistent dataIdentifier 0xF191
        uint8_t REQ[] = {0x22, 0xF1, 0x91};
        UDSTpSend(mock_client,  REQ, sizeof(REQ), NULL);
        uint8_t RESP[] = {0x7F, 0x22, 0x31};
        EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) == sizeof(RESP), srv.p2_ms)
        TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RESP, sizeof(RESP));
    }
}
