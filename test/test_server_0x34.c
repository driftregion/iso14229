#include "test/test.h"

uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    TEST_INT_EQUAL(ev, UDS_SRV_EVT_RequestDownload);
    UDSRequestDownloadArgs_t *r = (UDSRequestDownloadArgs_t *)arg;
    TEST_INT_EQUAL(0x11, r->dataFormatIdentifier);
    TEST_PTR_EQUAL((void *)0x602000, r->addr);
    TEST_INT_EQUAL(0x00FFFF, r->size);
    TEST_INT_EQUAL(r->maxNumberOfBlockLength, UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH);
    r->maxNumberOfBlockLength = 0x0081;
    return kPositiveResponse;
}

int main() {
    { // case 0: No handler
        UDSSess_t mock_client;
        UDSServer_t srv;
        ENV_SERVER_INIT(srv);
        ENV_SESS_INIT(mock_client);

        // when no handler function is installed, sending this request to the server
        uint8_t REQ[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
        EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));

        // should return a kServiceNotSupported response
        uint8_t RESP[] = {0x7F, 0x34, 0x11};
        ENV_RunMillis(50);
        TEST_MEMORY_EQUAL(mock_client.recv_buf, RESP, sizeof(RESP));
        TPMockReset();
    }

    { // case 1: handler installed
        UDSSess_t mock_client;
        UDSServer_t srv;
        ENV_SERVER_INIT(srv);
        ENV_SESS_INIT(mock_client);
        // when a handler is installed that implements UDS-1:2013 Table 415
        srv.fn = fn;

        // sending this request to the server
        uint8_t REQ[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
        EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));

        // should receive a positive response matching UDS-1:2013 Table 415
        uint8_t RESP[] = {0x74, 0x20, 0x00, 0x81};
        ENV_RunMillis(50);
        TEST_MEMORY_EQUAL(mock_client.recv_buf, RESP, sizeof(RESP));
        TPMockReset();
    }
}
