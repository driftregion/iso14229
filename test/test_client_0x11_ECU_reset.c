#include "test/test.h"

int main() {
    UDSClient_t client;

    const uint8_t GOOD[] = {0x51, 0x01};
    const uint8_t BAD_SID[] = {0x50, 0x01};
    const uint8_t TOO_SHORT[] = {0x51};
    const uint8_t BAD_SUBFUNC[] = {0x51, 0x02};
    const uint8_t NEG[] = {0x7F, 0x11, 0x10};

#define CASE(d1, opt, err)                                                                         \
    {                                                                                              \
        .tag = "client options:" #opt ", server resp: " #d1 ", expected_err: " #err, .resp = d1,   \
        .resp_len = sizeof(d1), .options = opt, .expected_err = err                                \
    }

    struct {
        const char *tag;
        const uint8_t *resp;
        size_t resp_len;
        uint8_t options;
        UDSErr_t expected_err;
    } p[] = {
        CASE(GOOD, 0, UDS_OK),
        CASE(BAD_SID, 0, UDS_ERR_SID_MISMATCH),
        CASE(TOO_SHORT, 0, UDS_ERR_RESP_TOO_SHORT),
        CASE(BAD_SUBFUNC, 0, UDS_ERR_SUBFUNCTION_MISMATCH),
        CASE(NEG, 0, UDS_OK),
        CASE(NEG, UDS_NEG_RESP_IS_ERR, UDS_ERR_NEG_RESP),
        CASE(GOOD, UDS_NEG_RESP_IS_ERR, UDS_OK),
        CASE(GOOD, UDS_SUPPRESS_POS_RESP, UDS_OK), // Should this case pass?
    };

    for (size_t i = 0; i < sizeof(p) / sizeof(p[0]); i++) {
        ENV_CLIENT_INIT(client);
        UDSTpHandle_t *mock_srv = ENV_GetMockTp("server");
        printf("test %ld: %s\n", i, p[i].tag);

        // when the client sends a ECU reset request with these options
        client.options = p[i].options;
        UDSSendECUReset(&client, kHardReset);

        // and the server responds with this message
        UDSTpSend(mock_srv, p[i].resp, p[i].resp_len, NULL);

        // then the client should receive a response with this error code
        ENV_RunMillis(50);
        TEST_INT_EQUAL(client.state, kRequestStateIdle);
        TEST_INT_EQUAL(client.err, p[i].expected_err);
        // assert(client.err == p[i].expected_err);
        TPMockReset();
    }
}