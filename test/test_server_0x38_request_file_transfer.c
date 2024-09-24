#include "test/test.h"

uint8_t fn_addfile(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    TEST_INT_EQUAL(ev, UDS_SRV_EVT_RequestFileTransfer);
    UDSRequestFileTransferArgs_t *r = (UDSRequestFileTransferArgs_t *)arg;
    TEST_INT_EQUAL(0x01, r->modeOfOperation);
    TEST_INT_EQUAL(18, r->filePathLen);
    TEST_MEMORY_EQUAL((void *)"/data/testfile.zip", r->filePath, r->filePathLen);
    TEST_INT_EQUAL(0x00, r->dataFormatIdentifier);
    TEST_INT_EQUAL(0x112233, r->fileSizeUnCompressed);
    TEST_INT_EQUAL(0x001122, r->fileSizeCompressed);
    r->maxNumberOfBlockLength = 0x0081;
    return kPositiveResponse;
}

uint8_t fn_delfile(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    TEST_INT_EQUAL(ev, UDS_SRV_EVT_RequestFileTransfer);
    UDSRequestFileTransferArgs_t *r = (UDSRequestFileTransferArgs_t *)arg;
    TEST_INT_EQUAL(0x02, r->modeOfOperation);
    TEST_INT_EQUAL(18, r->filePathLen);
    TEST_MEMORY_EQUAL((void *)"/data/testfile.zip", r->filePath, r->filePathLen);
    return kPositiveResponse;
}

int main() {
    { // case 0: No handler
        UDSTpHandle_t *mock_client = ENV_TpNew("client");
        UDSServer_t srv;
        ENV_SERVER_INIT(srv);

        // when no handler function is installed, sending this request to the server
        uint8_t ADDFILE_REQUEST[] = {0x38, 0x01, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x2F, 0x74, 0x65,
                                            0x73, 0x74, 0x66, 0x69, 0x6C, 0x65, 0x2E, 0x7A, 0x69, 0x70, 0x00, 0x03, 
                                            0x11, 0x22, 0x33, 0x00, 0x11, 0x22};
        UDSTpSend(mock_client, ADDFILE_REQUEST, sizeof(ADDFILE_REQUEST), NULL);

        // should return a kServiceNotSupported response
        uint8_t RESP[] = {0x7F, 0x38, 0x11};
        EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) > 0, srv.p2_ms);
        TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RESP, sizeof(RESP));
        TPMockReset();
    }

    { // case 1: add file
        UDSTpHandle_t *mock_client = ENV_TpNew("client");
        UDSServer_t srv;
        ENV_SERVER_INIT(srv);
        // when a handler is installed that implements UDS-1:2013 Table 435
        srv.fn = fn_addfile;

        // sending this request to the server
        uint8_t ADDFILE_REQUEST[] = {0x38, 0x01, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x2F, 0x74, 0x65,
                                            0x73, 0x74, 0x66, 0x69, 0x6C, 0x65, 0x2E, 0x7A, 0x69, 0x70, 0x00, 0x03, 
                                            0x11, 0x22, 0x33, 0x00, 0x11, 0x22};
        UDSTpSend(mock_client, ADDFILE_REQUEST, sizeof(ADDFILE_REQUEST), NULL);

        // should receive a positive response matching UDS-1:2013 Table 435
        uint8_t RESP[] = {0x78, 0x01, 0x02, 0x00, 0x81};
        EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) > 0, srv.p2_ms);
        TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RESP, sizeof(RESP));
        TPMockReset();
    }
    { // case 2: delete file
        UDSTpHandle_t *mock_client = ENV_TpNew("client");
        UDSServer_t srv;
        ENV_SERVER_INIT(srv);
        // when a handler is installed that implements UDS-1:2013 Table 435
        srv.fn = fn_delfile;

        // sending this request to the server
        const uint8_t DELETEFILE_REQUEST[] = {0x38, 0x02, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x2F, 0x74, 0x65,
                                            0x73, 0x74, 0x66, 0x69, 0x6C, 0x65, 0x2E, 0x7A, 0x69, 0x70};
        UDSTpSend(mock_client, DELETEFILE_REQUEST, sizeof(DELETEFILE_REQUEST), NULL);

        // should receive a positive response matching UDS-1:2013 Table 435
        uint8_t RESP[] = {0x78, 0x02};
        EXPECT_IN_APPROX_MS(UDSTpGetRecvLen(mock_client) > 0, srv.p2_ms);
        TEST_MEMORY_EQUAL(UDSTpGetRecvBuf(mock_client, NULL), RESP, sizeof(RESP));
        TPMockReset();
    }
}
