#include "test/test.h"

uint8_t fn_callCount = 0;
uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    switch (ev) {
    case UDS_SRV_EVT_EcuReset:
        fn_callCount += 1;
        return kPositiveResponse;
    default:
        TEST_INT_EQUAL(UDS_SRV_EVT_DoScheduledReset, ev);
        return kPositiveResponse;
    }
}

int main() {
    UDSSess_t mock_client;
    UDSServer_t srv;
    ENV_SERVER_INIT(srv);
    ENV_SESS_INIT(mock_client);
    srv.fn = fn;

    const uint8_t REQ[] = {0x11, 0x01};
    const uint8_t RESP[] = {0x51, 0x01};

    // Sending the first ECU reset should result in a response
    EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));
    ENV_RunMillis(50);
    TEST_MEMORY_EQUAL(mock_client.recv_buf, RESP, sizeof(RESP));

    mock_client.recv_size = 0;

    // Sending subsequent ECU reset requests should not receive any response
    EXPECT_OK(UDSSessSend(&mock_client, REQ, sizeof(REQ)));
    ENV_RunMillis(5000);
    TEST_INT_EQUAL(mock_client.recv_size, 0);

    // The ECU reset handler should have been called once.
    TEST_INT_EQUAL(fn_callCount, 1);
}
