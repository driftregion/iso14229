#include "test/test.h"

static UDSErr_t err = UDS_OK;
static int fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    switch (evt) {
        case UDS_EVT_Err:
            assert_true(0);
    }
}

int main() {
    UDSClient_t client;
    ENV_CLIENT_INIT(client);
    client.fn = fn;

    // Setting the suppressPositiveResponse flag before sending a request
    client.options |= UDS_SUPPRESS_POS_RESP;
    UDSSendECUReset(&client, kHardReset);

    // and not receiving a response after approximately p2 ms
    ENV_RunMillis(UDS_CLIENT_DEFAULT_P2_MS + 10);

    // There should be no error and the client should be idle
    TEST_INT_EQUAL(kRequestStateIdle, client.state);
}
