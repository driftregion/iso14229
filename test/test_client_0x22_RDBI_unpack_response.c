#include "test/test.h"

int main() {
    uint8_t RESP[] = {0x72, 0x12, 0x34, 0x00, 0x00, 0xAA, 0x00, 0x56, 0x78, 0xAA, 0xBB};
    uint8_t buf[4];
    uint16_t offset = 0;
    int err = 0;
    err = UDSUnpackRDBIResponse(RESP, sizeof(RESP), 0x1234, buf, 4, &offset);
    TEST_INT_EQUAL(err, UDS_OK);
    uint32_t d0 = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
    TEST_INT_EQUAL(d0, 0x0000AA00);
    err = UDSUnpackRDBIResponse(RESP, sizeof(RESP), 0x1234, buf, 2, &offset);
    TEST_INT_EQUAL(err, UDS_ERR_DID_MISMATCH);
    err = UDSUnpackRDBIResponse(RESP, sizeof(RESP), 0x5678, buf, 20, &offset);
    TEST_INT_EQUAL(err, UDS_ERR_RESP_TOO_SHORT);
    err = UDSUnpackRDBIResponse(RESP, sizeof(RESP), 0x5678, buf, 2, &offset);
    TEST_INT_EQUAL(err, UDS_OK);
    uint16_t d1 = (buf[0] << 8) + buf[1];
    TEST_INT_EQUAL(d1, 0xAABB);
    err = UDSUnpackRDBIResponse(RESP, sizeof(RESP), 0x5678, buf, 1, &offset);
    TEST_INT_EQUAL(err, UDS_ERR_RESP_TOO_SHORT);
    TEST_INT_EQUAL(offset, sizeof(RESP));
}