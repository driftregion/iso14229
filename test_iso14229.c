#define UDS_TP UDS_TP_CUSTOM
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include "iso14229.h"
#include "tp/mock.h"

#define _ASSERT_INT_COND(a, b, cond)                                                               \
    {                                                                                              \
        int _a = a;                                                                                \
        int _b = b;                                                                                \
        if (!((_a)cond(_b))) {                                                                     \
            printf("%s:%d (%d %s %d)\n", __FILE__, __LINE__, _a, #cond, _b);                       \
            fflush(stdout);                                                                        \
            assert(a cond b);                                                                      \
        }                                                                                          \
    }

#define ASSERT_INT_LT(a, b) _ASSERT_INT_COND(a, b, <)
#define ASSERT_INT_LE(a, b) _ASSERT_INT_COND(a, b, <=)
#define ASSERT_INT_GE(a, b) _ASSERT_INT_COND(a, b, >=)
#define ASSERT_INT_EQUAL(a, b) _ASSERT_INT_COND(a, b, ==)
#define ASSERT_INT_NE(a, b) _ASSERT_INT_COND(a, b, !=)

#define ASSERT_PTR_EQUAL(a, b)                                                                     \
    {                                                                                              \
        const void *_a = a;                                                                        \
        const void *_b = b;                                                                        \
        if ((_a) != (_b)) {                                                                        \
            printf("%s:%d (%p != %p)\n", __FILE__, __LINE__, _a, _b);                              \
            fflush(stdout);                                                                        \
            assert(a == b);                                                                        \
        }                                                                                          \
    }

#define ASSERT_MEMORY_EQUAL(a, b, len)                                                             \
    {                                                                                              \
        const uint8_t *_a = (const uint8_t *)a;                                                    \
        const uint8_t *_b = (const uint8_t *)b;                                                    \
        if (memcmp(_a, _b, len)) {                                                                 \
            printf("A:");                                                                          \
            for (unsigned int i = 0; i < len; i++) {                                               \
                printf("%02x,", _a[i]);                                                            \
            }                                                                                      \
            printf(" (%s)\nB:", #a);                                                               \
            for (unsigned int i = 0; i < len; i++) {                                               \
                printf("%02x,", _b[i]);                                                            \
            }                                                                                      \
            printf(" (%s)\n", #b);                                                                 \
            fflush(stdout);                                                                        \
            assert(0);                                                                             \
        }                                                                                          \
    }

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"

#define CLIENT_TARGET_ADDR (0x7E0U)
#define CLIENT_TARGET_ADDR_FUNC (0x7DFU)
#define CLIENT_SOURCE_ADDR (0x7E8U)

#define SERVER_TARGET_ADDR (0x7E8U)
#define SERVER_SOURCE_ADDR (0x7E0U)
#define SERVER_SOURCE_ADDR_FUNC (0x7DFU)

/*
    ctx.server_tp = (struct mocktransport){.hdl = {.recv = mock_transport_recv,                    \
                                                   .send = mock_transport_send,                    \
                                                   .poll = mock_transport_poll},                   \
                                           .tag = "server"};                                       \
    ctx.client_tp = (struct mocktransport){.hdl = {.recv = mock_transport_recv,                    \
                                                   .send = mock_transport_send,                    \
                                                   .poll = mock_transport_poll},                   \
                                           .tag = "client"};                                       \
    UDSClientInit(&ctx.client, &(UDSClientConfig_t){.tp = &ctx.client_tp.hdl});                    \
    UDSServerInit(&ctx.server, &(UDSServerConfig_t){.tp = &ctx.server_tp.hdl});
    */

#define SERVER_ONLY 0
#define CLIENT_ONLY 1

#define _TEST_SETUP_SILENT(test_type, param_str)                                                   \
    memset(&ctx, 0, sizeof(ctx));                                                                  \
    ctx.func_name = __PRETTY_FUNCTION__;                                                           \
    if (SERVER_ONLY == test_type) {                                                                \
        UDSServerInit(&ctx.server, &(UDSServerConfig_t){                                           \
                                       .tp = TPMockNew("server"),                                  \
                                       .source_addr = SERVER_SOURCE_ADDR,                          \
                                       .target_addr = SERVER_TARGET_ADDR,                          \
                                       .source_addr_func = SERVER_SOURCE_ADDR_FUNC,                \
                                   });                                                             \
        ctx.mock_tp = TPMockNew("mock_client");                                                    \
    }                                                                                              \
    if (CLIENT_ONLY == test_type) {                                                                \
        UDSClientInit(&ctx.client, &(UDSClientConfig_t){                                           \
                                       .tp = TPMockNew("client"),                                  \
                                       .target_addr = CLIENT_TARGET_ADDR,                          \
                                       .source_addr = CLIENT_SOURCE_ADDR,                          \
                                       .target_addr_func = CLIENT_TARGET_ADDR_FUNC,                \
                                   });                                                             \
        ctx.mock_tp = TPMockNew("mock_server");                                                    \
    }                                                                                              \
    char logfilename[256] = {0};                                                                   \
    snprintf(logfilename, sizeof(logfilename), "%s%s.log", ctx.func_name, param_str);              \
    TPMockLogToFile(logfilename);

#define TEST_SETUP(test_type)                                                                      \
    _TEST_SETUP_SILENT(test_type, "");                                                             \
    printf("%s\n", ctx.func_name);

#define TEST_SETUP_PARAMETRIZED(test_type, params_list)                                            \
    for (size_t i = 0; i < sizeof(params_list) / sizeof(params_list[0]); i++) {                    \
        char _param_str[128];                                                                      \
        snprintf(_param_str, sizeof(_param_str), "%s_p_%ld_%s", ctx.func_name, i,                  \
                 (*(char **)(&(params_list[i]))));                                                 \
        _TEST_SETUP_SILENT(test_type, _param_str);                                                 \
        printf("%s\n", _param_str);

#define TEST_TEARDOWN_PARAMETRIZED()                                                               \
    TPMockReset();                                                                                 \
    printf(ANSI_BOLD "OK [p:%ld]\n" ANSI_RESET, i);                                                \
    }

#define TEST_TEARDOWN()                                                                            \
    {                                                                                              \
        TPMockReset();                                                                             \
        printf(ANSI_BOLD "OK\n" ANSI_RESET);                                                       \
    }

// TODO: parameterize and fuzz this
#define DEFAULT_ISOTP_BUFSIZE (2048U)

struct MockTransport {
    UDSTpHandle_t hdl;
    uint8_t recv_buf[DEFAULT_ISOTP_BUFSIZE];
    uint8_t send_buf[DEFAULT_ISOTP_BUFSIZE];
    uint16_t recv_size;
    uint16_t send_size;
    UDSTpAddr_t recv_ta_type;
    UDSTpAddr_t send_ta_type;
    UDSTpStatus_t status;
    const char *tag;
};

static void printhex(const uint8_t *addr, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x,", addr[i]);
    }
    printf("\n");
}

// static ssize_t mock_transport_recv(UDSTpHandle_t *hdl, UDSSDU_t *msg) {
//     assert(hdl);
//     struct MockTransport *tp = (struct MockTransport *)hdl;
//     size_t size = tp->recv_size;

//     if (msg->A_DataBufSize < size) {
//         return -ENOBUFS;
//     }

//     if (size) {
//         memmove((void *)msg->A_Data, tp->recv_buf, size);
//         tp->recv_size = 0;
//         memset(tp->recv_buf, 0, sizeof(tp->recv_buf));
//         printf(ANSI_BRIGHT_MAGENTA "<-%s_tp_recv-%04d- [%02ld] ", tp->tag, UDSMillis(), size);
//         printhex(msg->A_Data, size);
//         printf(ANSI_RESET);
//     }
//     return size;
// }

// static ssize_t mock_transport_send(UDSTpHandle_t *hdl, UDSSDU_t *msg) {
//     assert(hdl);
//     struct MockTransport *tp = (struct MockTransport *)hdl;
//     printf(ANSI_BRIGHT_GREEN "--%s_tp_send-%04d->[%02d] ", tp->tag, UDSMillis(), msg->A_Length);
//     printhex(msg->A_Data, msg->A_Length);
//     printf(ANSI_RESET);
//     assert(msg->A_Length); // why send zero?
//     memmove(tp->send_buf, msg->A_Data, msg->A_Length);
//     tp->send_size = msg->A_Length;
//     return msg->A_Length;
// }

// static UDSTpStatus_t mock_transport_poll(UDSTpHandle_t *hdl) {
//     assert(hdl);
//     struct MockTransport *tp = (struct MockTransport *)hdl;
//     return tp->status;
// }

typedef struct {
    UDSServer_t server;
    UDSClient_t client;
    UDSTpHandle_t *mock_tp;
    uint8_t mock_recv_buf[DEFAULT_ISOTP_BUFSIZE];
    struct MockTransport client_tp;
    uint32_t time_ms;
    uint32_t deadline;
    int call_count;
    const char *func_name;
} Ctx_t;

Ctx_t ctx;

uint32_t UDSMillis() { return ctx.time_ms; }

static void poll_ctx(Ctx_t *ctx) {
    if (ctx->server.tp) {
        UDSServerPoll(&ctx->server);
    }
    if (ctx->client.tp) {
        UDSClientPoll(&ctx->client);
    }
    ctx->time_ms++;
}

/*
    memmove(&ctx.server_tp.recv_buf, d1, sizeof(d1));                                              \
    ctx.server_tp.recv_ta_type = reqType;                                                          \
    ctx.server_tp.recv_size = sizeof(d1);                                                          \
    poll_ctx(&ctx);
*/

#define SEND_TO_SERVER(d1, reqType)                                                                \
    {                                                                                              \
        UDSSDU_t msg = {                                                                           \
            .A_Mtype = UDS_A_MTYPE_DIAG,                                                           \
            .A_Data = d1,                                                                          \
            .A_Length = sizeof(d1),                                                                \
            .A_SA = CLIENT_SOURCE_ADDR,                                                            \
            .A_TA =                                                                                \
                reqType == UDS_A_TA_TYPE_PHYSICAL ? SERVER_SOURCE_ADDR : SERVER_SOURCE_ADDR_FUNC,  \
            .A_TA_Type = (int)reqType,                                                             \
        };                                                                                         \
        ctx.mock_tp->send(ctx.mock_tp, &msg);                                                      \
    }

#define ASSERT_CLIENT_SENT(d1, reqType)                                                            \
    {                                                                                              \
        UDSSDU_t msg = {                                                                           \
            .A_DataBufSize = DEFAULT_ISOTP_BUFSIZE,                                                \
            .A_Data = ctx.mock_recv_buf,                                                           \
        };                                                                                         \
        int recv_len = ctx.mock_tp->recv(ctx.mock_tp, &msg);                                       \
        ASSERT_INT_EQUAL(recv_len, sizeof(d1));                                                    \
        ASSERT_MEMORY_EQUAL(ctx.mock_recv_buf, d1, sizeof(d1));                                    \
        if (reqType == UDS_A_TA_TYPE_PHYSICAL) {                                                   \
            ASSERT_INT_EQUAL(msg.A_TA, SERVER_SOURCE_ADDR);                                        \
        } else if (reqType == UDS_A_TA_TYPE_FUNCTIONAL) {                                          \
            ASSERT_INT_EQUAL(msg.A_TA, SERVER_SOURCE_ADDR_FUNC);                                   \
        } else {                                                                                   \
            assert(0);                                                                             \
        }                                                                                          \
    }

// send data to the client
static void send_to_client(const uint8_t *d1, size_t len, UDSTpAddr_t reqType) {
    assert(len <= sizeof(ctx.client_tp.recv_buf));
    ctx.mock_tp->send(ctx.mock_tp, &(UDSSDU_t){
                                       .A_Mtype = UDS_A_MTYPE_DIAG,
                                       .A_Data = d1,
                                       .A_Length = len,
                                       .A_SA = SERVER_SOURCE_ADDR,
                                       .A_TA = SERVER_TARGET_ADDR,
                                       .A_TA_Type = (int)reqType,
                                   });
    // memmove(&ctx.client_tp.recv_buf, d1, len);
    // ctx.client_tp.recv_ta_type = reqType;
    // ctx.client_tp.recv_size = len;
    poll_ctx(&ctx);
}

#define SEND_TO_CLIENT(d1, reqType) send_to_client(d1, sizeof(d1), reqType);
/*
        while (0 == ctx.server_tp.send_size) {                                                     \
            poll_ctx(&ctx);                                                                        \
            ASSERT_INT_LE(ctx.time_ms, deadline);                                                  \
        }                                                                                          \
        */

// expect a server response within a timeout
#define EXPECT_RESPONSE_WITHIN_MILLIS(d1, reqType, timeout_ms)                                     \
    {                                                                                              \
        uint32_t deadline = ctx.time_ms + timeout_ms + 1;                                          \
        UDSSDU_t msg = {                                                                           \
            .A_DataBufSize = DEFAULT_ISOTP_BUFSIZE,                                                \
            .A_Data = ctx.mock_recv_buf,                                                           \
        };                                                                                         \
        while (0 == ctx.mock_tp->recv(ctx.mock_tp, &msg)) {                                        \
            poll_ctx(&ctx);                                                                        \
            printf("%d, %d, %d\n", UDSMillis(), ctx.time_ms, deadline);                            \
            ASSERT_INT_LE(ctx.time_ms, deadline);                                                  \
        }                                                                                          \
        printhex(msg.A_Data, msg.A_Length);                                                        \
        ASSERT_INT_EQUAL(msg.A_Length, sizeof(d1));                                                \
        ASSERT_MEMORY_EQUAL(msg.A_Data, d1, sizeof(d1));                                           \
        ASSERT_INT_EQUAL((int)msg.A_TA_Type, reqType);                                             \
    }

// expect no server response within a timeout
#define EXPECT_NO_RESPONSE_FOR_MILLIS(timeout_ms)                                                  \
    {                                                                                              \
        uint32_t deadline = ctx.time_ms + timeout_ms;                                              \
        while (ctx.time_ms <= deadline) {                                                          \
            poll_ctx(&ctx);                                                                        \
            UDSSDU_t msg = {                                                                       \
                .A_DataBufSize = DEFAULT_ISOTP_BUFSIZE,                                            \
                .A_Data = ctx.mock_recv_buf,                                                       \
            };                                                                                     \
            int resp_len = ctx.mock_tp->recv(ctx.mock_tp, &msg);                                   \
            ASSERT_INT_EQUAL(resp_len, 0);                                                         \
        }                                                                                          \
    }

void testServer0x10DiagSessCtrlIsDisabledByDefault() {
    TEST_SETUP(SERVER_ONLY);
    const uint8_t REQ[] = {0x10, 0x02};
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
    const uint8_t RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_RESPONSE_WITHIN_MILLIS(RESP, UDS_A_TA_TYPE_PHYSICAL, 50);
    TEST_TEARDOWN();
}

void testServer0x10DiagSessCtrlFunctionalRequest() {
    TEST_SETUP(SERVER_ONLY);
    // sending a diagnostic session control request functional broadcast
    const uint8_t REQ[] = {0x10, 0x03};
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_FUNCTIONAL);
    // should receive a physical response
    const uint8_t RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_RESPONSE_WITHIN_MILLIS(RESP, UDS_A_TA_TYPE_PHYSICAL, 50);
    TEST_TEARDOWN();
}

uint8_t fn1_callCount = 0;
uint8_t fn1(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    switch (ev) {
    case UDS_SRV_EVT_EcuReset:
        fn1_callCount += 1;
        return kPositiveResponse;
    default:
        ASSERT_INT_EQUAL(UDS_SRV_EVT_DoScheduledReset, ev);
        return kPositiveResponse;
    }
}

// Special-case of ECU reset service
// ISO-14229-1 2013 9.3.1:
// on the behaviour of the ECU from the time following the positive response message to the ECU
// reset request: It is recommended that during this time the ECU does not accept any request
// messages and send any response messages.
void testServer0x11DoesNotSendOrReceiveMessagesAfterECUReset() {
    TEST_SETUP(SERVER_ONLY);
    ctx.server.fn = fn1;

    const uint8_t REQ[] = {0x11, 0x01};
    const uint8_t RESP[] = {0x51, 0x01};

    // Sending the first ECU reset should result in a response
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_RESPONSE_WITHIN_MILLIS(RESP, UDS_A_TA_TYPE_PHYSICAL, 50);
    // Sending subsequent ECU reset requests should not receive any response
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_NO_RESPONSE_FOR_MILLIS(5000);

    // The ECU reset handler should have been called once.
    ASSERT_INT_EQUAL(fn1_callCount, 1);
    TEST_TEARDOWN();
}

uint8_t fn2(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    ASSERT_INT_EQUAL(UDS_SRV_EVT_ReadDataByIdent, ev);
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

void testServer0x22RDBI1() {
    TEST_SETUP(SERVER_ONLY);
    ctx.server.fn = fn2;
    {
        uint8_t REQ[] = {0x22, 0xF1, 0x90};
        SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
        uint8_t RESP[] = {0x62, 0xF1, 0x90, 0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30,
                          0x34, 0x33, 0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
        EXPECT_RESPONSE_WITHIN_MILLIS(RESP, UDS_A_TA_TYPE_PHYSICAL, 50);
    }
    {
        uint8_t REQ[] = {0x22, 0xF1, 0x91};
        SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
        uint8_t RESP[] = {0x7F, 0x22, 0x31};
        EXPECT_RESPONSE_WITHIN_MILLIS(RESP, UDS_A_TA_TYPE_PHYSICAL, 50);
    }
    TEST_TEARDOWN();
}

uint8_t fn10(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    ASSERT_INT_EQUAL(ev, UDS_SRV_EVT_ReadMemByAddr);
    UDSReadMemByAddrArgs_t *r = (UDSReadMemByAddrArgs_t *)arg;
    //                                      1 2 3 4 5 6 7 8
    ASSERT_PTR_EQUAL(r->memAddr, (void *)0x000055555555f0c8);
    ASSERT_INT_EQUAL(r->memSize, 4);
    uint8_t FakeData[4] = {0x01, 0x02, 0x03, 0x04};
    return r->copy(srv, FakeData, r->memSize);
}

void testServer0x23ReadMemoryByAddress() {
    TEST_SETUP(SERVER_ONLY);
    ctx.server.fn = fn10;
    uint8_t REQ[] = {0x23, 0x18, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0xf0, 0xc8, 0x04};
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
    uint8_t RESP[] = {0x63, 0x01, 0x02, 0x03, 0x04};
    EXPECT_RESPONSE_WITHIN_MILLIS(RESP, UDS_A_TA_TYPE_PHYSICAL, 50);
    TEST_TEARDOWN();
}

uint8_t fn4(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    switch (ev) {
    case UDS_SRV_EVT_SecAccessRequestSeed: {
        UDSSecAccessRequestSeedArgs_t *r = (UDSSecAccessRequestSeedArgs_t *)arg;
        const uint8_t seed[] = {0x36, 0x57};
        ASSERT_INT_NE(r->level, srv->securityLevel);
        return r->copySeed(srv, seed, sizeof(seed));
        break;
    }
    case UDS_SRV_EVT_SecAccessValidateKey: {
        UDSSecAccessValidateKeyArgs_t *r = (UDSSecAccessValidateKeyArgs_t *)arg;
        const uint8_t expected_key[] = {0xC9, 0xA9};
        ASSERT_INT_EQUAL(r->len, sizeof(expected_key));
        ASSERT_MEMORY_EQUAL(r->key, expected_key, sizeof(expected_key));
        break;
    }
    default:
        assert(0);
    }
    return kPositiveResponse;
}

// UDS-1 2013 9.4.5.2
// UDS-1 2013 9.4.5.3
void testServer0x27SecurityAccess() {
    TEST_SETUP(SERVER_ONLY);
    ctx.server.fn = fn4;

    // the server security level after initialization should be 0
    ASSERT_INT_EQUAL(ctx.server.securityLevel, 0);

    // sending a seed request should get this response
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    const uint8_t SEED_RESPONSE[] = {0x67, 0x01, 0x36, 0x57};
    SEND_TO_SERVER(SEED_REQUEST, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_RESPONSE_WITHIN_MILLIS(SEED_RESPONSE, UDS_A_TA_TYPE_PHYSICAL, 50);

    // subsequently sending an unlock request should get this response
    const uint8_t UNLOCK_REQUEST[] = {0x27, 0x02, 0xC9, 0xA9};
    const uint8_t UNLOCK_RESPONSE[] = {0x67, 0x02};
    SEND_TO_SERVER(UNLOCK_REQUEST, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_RESPONSE_WITHIN_MILLIS(UNLOCK_RESPONSE, UDS_A_TA_TYPE_PHYSICAL, 50);

    // sending the same seed request should now result in the "already unlocked" response
    const uint8_t ALREADY_UNLOCKED_RESPONSE[] = {0x67, 0x01, 0x00, 0x00};
    SEND_TO_SERVER(SEED_REQUEST, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_RESPONSE_WITHIN_MILLIS(ALREADY_UNLOCKED_RESPONSE, UDS_A_TA_TYPE_PHYSICAL, 50);

    // Additionally, the security level should now be 1
    ASSERT_INT_EQUAL(ctx.server.securityLevel, 1);
    TEST_TEARDOWN();
}

static uint8_t ReturnRCRRP(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    return kRequestCorrectlyReceived_ResponsePending;
}
static uint8_t ReturnPositiveResponse(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    return kPositiveResponse;
}

// ISO-14229-1 2013 Table A.1 Byte Value 0x78: requestCorrectlyReceived-ResponsePending
// "This NRC is in general supported by each diagnostic service".
void testServer0x31RCRRP() {
    TEST_SETUP(SERVER_ONLY);
    // When a server handler func initially returns RRCRP
    ctx.server.fn = ReturnRCRRP;

    // sending a request to the server should return RCRRP
    const uint8_t REQUEST[] = {0x31, 0x01, 0x12, 0x34};
    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78};
    SEND_TO_SERVER(REQUEST, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_RESPONSE_WITHIN_MILLIS(RCRRP, UDS_A_TA_TYPE_PHYSICAL, 50)

    // The server should again respond within p2_star ms, and keep responding
    EXPECT_RESPONSE_WITHIN_MILLIS(RCRRP, UDS_A_TA_TYPE_PHYSICAL, 50)
    EXPECT_RESPONSE_WITHIN_MILLIS(RCRRP, UDS_A_TA_TYPE_PHYSICAL, 50)
    EXPECT_RESPONSE_WITHIN_MILLIS(RCRRP, UDS_A_TA_TYPE_PHYSICAL, 50)
    EXPECT_RESPONSE_WITHIN_MILLIS(RCRRP, UDS_A_TA_TYPE_PHYSICAL, 50)
    EXPECT_RESPONSE_WITHIN_MILLIS(RCRRP, UDS_A_TA_TYPE_PHYSICAL, 50)
    EXPECT_RESPONSE_WITHIN_MILLIS(RCRRP, UDS_A_TA_TYPE_PHYSICAL, 50)

    // When the server handler func now returns a positive response
    ctx.server.fn = ReturnPositiveResponse;

    // the server's next response should be a positive one
    const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
    EXPECT_RESPONSE_WITHIN_MILLIS(POSITIVE_RESPONSE, UDS_A_TA_TYPE_PHYSICAL, 50)

    TEST_TEARDOWN();
}

void testServer0x34NotEnabled() {
    TEST_SETUP(SERVER_ONLY);
    // when no handler function is installed, sending this request to the server
    const uint8_t IN[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    SEND_TO_SERVER(IN, UDS_A_TA_TYPE_PHYSICAL);

    // should return a kServiceNotSupported response
    const uint8_t OUT[] = {0x7F, 0x34, 0x11};
    EXPECT_RESPONSE_WITHIN_MILLIS(OUT, UDS_A_TA_TYPE_PHYSICAL, 50);
    TEST_TEARDOWN();
}

uint8_t fn7(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    ASSERT_INT_EQUAL(ev, UDS_SRV_EVT_RequestDownload);
    UDSRequestDownloadArgs_t *r = (UDSRequestDownloadArgs_t *)arg;
    ASSERT_INT_EQUAL(0x11, r->dataFormatIdentifier);
    ASSERT_PTR_EQUAL((void *)0x602000, r->addr);
    ASSERT_INT_EQUAL(0x00FFFF, r->size);
    ASSERT_INT_EQUAL(r->maxNumberOfBlockLength, UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH);
    r->maxNumberOfBlockLength = 0x0081;
    return kPositiveResponse;
}

void testServer0x34() {
    TEST_SETUP(SERVER_ONLY);
    // when a handler is installed that implements UDS-1:2013 Table 415
    ctx.server.fn = fn7;

    // sending this request to the server
    uint8_t REQ[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);

    // should receive a positive response matching UDS-1:2013 Table 415
    uint8_t RESP[] = {0x74, 0x20, 0x00, 0x81};
    EXPECT_RESPONSE_WITHIN_MILLIS(RESP, UDS_A_TA_TYPE_PHYSICAL, 50)
    TEST_TEARDOWN();
}

/* UDS-1 2013 Table 72 */
void testServer0x3ESuppressPositiveResponse() {
    TEST_SETUP(SERVER_ONLY);
    ctx.server.fn = ReturnPositiveResponse;
    // when the suppressPositiveResponse bit is set
    const uint8_t REQ[] = {0x3E, 0x80};
    // there should be no response
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_NO_RESPONSE_FOR_MILLIS(5000);
    TEST_TEARDOWN();
}

void testServer0x83DiagnosticSessionControl() {
    TEST_SETUP(SERVER_ONLY);
    ctx.server.fn = ReturnPositiveResponse;
    // the server sessionType after initialization should be kDefaultSession.
    ASSERT_INT_EQUAL(ctx.server.sessionType, kDefaultSession);

    // When the suppressPositiveResponse bit is set, there should be no response.
    const uint8_t REQ[] = {0x10, 0x83};
    SEND_TO_SERVER(REQ, UDS_A_TA_TYPE_PHYSICAL);
    EXPECT_NO_RESPONSE_FOR_MILLIS(5000);
    // and the server sessionType should have changed
    ASSERT_INT_EQUAL(ctx.server.sessionType, kExtendedDiagnostic);
    TEST_TEARDOWN();
}

uint8_t fn9(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    ASSERT_INT_EQUAL(UDS_SRV_EVT_SessionTimeout, ev);
    ctx.call_count++;
    return kPositiveResponse;
}

void testServerSessionTimeout() {
    struct {
        const char *tag;
        uint8_t (*fn)(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg);
        uint8_t sessType;
        int expectedCallCount;
    } p[] = {
        {.tag = "no timeout", .fn = fn9, .sessType = kDefaultSession, .expectedCallCount = 0},
        {.tag = "timeout", .fn = fn9, .sessType = kProgrammingSession, .expectedCallCount = 1},
        {.tag = "no handler", .fn = NULL, .sessType = kProgrammingSession, .expectedCallCount = 0},
    };
    TEST_SETUP_PARAMETRIZED(SERVER_ONLY, p);
    ctx.server.fn = p[i].fn;
    ctx.server.sessionType = p[i].sessType;
    while (ctx.time_ms < 5000)
        poll_ctx(&ctx);
    ASSERT_INT_GE(ctx.call_count, p[i].expectedCallCount);
    TEST_TEARDOWN_PARAMETRIZED();
}

#define POLL_UNTIL_TIME_MS(abs_time_ms)                                                            \
    while (ctx.time_ms < (abs_time_ms))                                                            \
    poll_ctx(&ctx)

#define POLL_FOR_MS(duration_ms)                                                                   \
    {                                                                                              \
        uint32_t start_time_ms = ctx.time_ms;                                                      \
        while (ctx.time_ms < (start_time_ms + duration_ms))                                        \
            poll_ctx(&ctx);                                                                        \
    }

void testClientP2TimeoutExceeded() {
    TEST_SETUP(CLIENT_ONLY);
    // when sending a request that receives no response
    UDSSendECUReset(&ctx.client, kHardReset);

    // before p2 ms has elapsed, the client should have no error
    POLL_UNTIL_TIME_MS(UDS_CLIENT_DEFAULT_P2_MS - 10);
    ASSERT_INT_EQUAL(UDS_OK, ctx.client.err);

    // after p2 ms has elapsed, the client should have a timeout error
    POLL_UNTIL_TIME_MS(UDS_CLIENT_DEFAULT_P2_MS + 10);
    ASSERT_INT_EQUAL(UDS_ERR_TIMEOUT, ctx.client.err);
    TEST_TEARDOWN();
}

void testClientP2TimeoutNotExceeded() {
    TEST_SETUP(CLIENT_ONLY);
    // a client that sends an request
    UDSSendECUReset(&ctx.client, kHardReset);

    // which receives a positive response
    const uint8_t POSITIVE_RESPONSE[] = {0x51, 0x01};
    SEND_TO_CLIENT(POSITIVE_RESPONSE, UDS_A_TA_TYPE_PHYSICAL);

    POLL_UNTIL_TIME_MS(UDS_CLIENT_DEFAULT_P2_MS + 10);
    // should return to the idle state
    ASSERT_INT_EQUAL(kRequestStateIdle, ctx.client.state);
    // and should have no error.
    ASSERT_INT_EQUAL(UDS_OK, ctx.client.err);
    TEST_TEARDOWN();
}

void testClientSuppressPositiveResponse() {
    TEST_SETUP(CLIENT_ONLY);
    // Setting the suppressPositiveResponse flag before sending a request
    ctx.client.options |= UDS_SUPPRESS_POS_RESP;
    UDSSendECUReset(&ctx.client, kHardReset);

    // and not receiving a response after approximately p2 ms
    POLL_UNTIL_TIME_MS(UDS_CLIENT_DEFAULT_P2_MS + 10);

    // should not result in an error.
    ASSERT_INT_EQUAL(UDS_OK, ctx.client.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, ctx.client.state);
    TEST_TEARDOWN();
}

void testClientBusy() {
    TEST_SETUP(CLIENT_ONLY);
    // Sending a request should not return an error
    ASSERT_INT_EQUAL(UDS_OK, UDSSendECUReset(&ctx.client, kHardReset));

    // unless there is an existing unresolved request
    ASSERT_INT_EQUAL(UDS_ERR_BUSY, UDSSendECUReset(&ctx.client, kHardReset));
    TEST_TEARDOWN();
}

void testClient0x11ECUReset() {
    TEST_SETUP(CLIENT_ONLY);
    const uint8_t GOOD[] = {0x51, 0x01};
    const uint8_t BAD_SID[] = {0x50, 0x01};
    const uint8_t TOO_SHORT[] = {0x51};
    const uint8_t BAD_SUBFUNC[] = {0x51, 0x02};
    const uint8_t NEG[] = {0x7F, 0x11, 0x10};
#define CASE(d1, opt, err)                                                                         \
    {                                                                                              \
        .tag = "resp: " #d1 ", expected_err: " #err, .resp = d1, .resp_len = sizeof(d1),           \
        .options = opt, .expected_err = err                                                        \
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
    };
#undef CASE
    TEST_SETUP_PARAMETRIZED(CLIENT_ONLY, p);
    // sending a request with these options
    ctx.client.options = p[i].options;
    UDSSendECUReset(&ctx.client, kHardReset);
    // that receives this response
    send_to_client(p[i].resp, p[i].resp_len, UDS_A_TA_TYPE_PHYSICAL);
    // should return to the idle state
    POLL_UNTIL_TIME_MS(UDS_CLIENT_DEFAULT_P2_MS + 10);
    ASSERT_INT_EQUAL(kRequestStateIdle, ctx.client.state);
    // with the expected error.
    ASSERT_INT_EQUAL(p[i].expected_err, ctx.client.err);
    TEST_TEARDOWN_PARAMETRIZED();
}

void testClient0x22RDBITxBufferTooSmall() {
    TEST_SETUP(CLIENT_ONLY);

    // attempting to send a request payload of 6 bytes
    uint16_t didList[] = {0x0001, 0x0002, 0x0003};

    // which is larger than the underlying buffer
    ctx.client.send_buf_size = 4;

    // should return an error
    ASSERT_INT_EQUAL(UDS_ERR_INVALID_ARG,
                     UDSSendRDBI(&ctx.client, didList, sizeof(didList) / sizeof(didList[0])))

    // and no data should be sent
    ASSERT_INT_EQUAL(ctx.client.send_size, 0);
    TEST_TEARDOWN();
}

void testClient0x22RDBIUnpackResponse() {
    TEST_SETUP(CLIENT_ONLY);
    uint8_t RESPONSE[] = {0x72, 0x12, 0x34, 0x00, 0x00, 0xAA, 0x00, 0x56, 0x78, 0xAA, 0xBB};
    UDSClient_t client;
    memmove(client.recv_buf, RESPONSE, sizeof(RESPONSE));
    client.recv_size = sizeof(RESPONSE);
    uint8_t buf[4];
    uint16_t offset = 0;
    int err = 0;
    err = UDSUnpackRDBIResponse(&client, 0x1234, buf, 4, &offset);
    ASSERT_INT_EQUAL(err, UDS_OK);
    uint32_t d0 = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
    ASSERT_INT_EQUAL(d0, 0x0000AA00);
    err = UDSUnpackRDBIResponse(&client, 0x1234, buf, 2, &offset);
    ASSERT_INT_EQUAL(err, UDS_ERR_DID_MISMATCH);
    err = UDSUnpackRDBIResponse(&client, 0x5678, buf, 20, &offset);
    ASSERT_INT_EQUAL(err, UDS_ERR_RESP_TOO_SHORT);
    err = UDSUnpackRDBIResponse(&client, 0x5678, buf, 2, &offset);
    ASSERT_INT_EQUAL(err, UDS_OK);
    uint16_t d1 = (buf[0] << 8) + buf[1];
    ASSERT_INT_EQUAL(d1, 0xAABB);
    err = UDSUnpackRDBIResponse(&client, 0x5678, buf, 1, &offset);
    ASSERT_INT_EQUAL(err, UDS_ERR_RESP_TOO_SHORT);
    ASSERT_INT_EQUAL(offset, sizeof(RESPONSE));
    TEST_TEARDOWN();
}

void testClient0x31RCRRP() {
    TEST_SETUP(CLIENT_ONLY);

    { // Case 1: RCRRP Timeout
        // When a request is sent
        UDSSendRoutineCtrl(&ctx.client, kStartRoutine, 0x1234, NULL, 0);

        // that receives an RCRRP response
        const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
        SEND_TO_CLIENT(RCRRP, UDS_A_TA_TYPE_PHYSICAL);

        // that remains unresolved at a time between p2 ms and p2 star ms
        POLL_FOR_MS(UDS_CLIENT_DEFAULT_P2_MS + 10);
        // the client should still be pending.
        ASSERT_INT_EQUAL(kRequestStateAwaitResponse, ctx.client.state)

        // after p2_star has elapsed, the client should timeout
        POLL_FOR_MS(UDS_CLIENT_DEFAULT_P2_STAR_MS + 10);
        ASSERT_INT_EQUAL(kRequestStateIdle, ctx.client.state)
        ASSERT_INT_EQUAL(ctx.client.err, UDS_ERR_TIMEOUT);
    }

    { // Case 2: Positive Response Received
        // When a request is sent
        UDSSendRoutineCtrl(&ctx.client, kStartRoutine, 0x1234, NULL, 0);

        // that receives an RCRRP response
        const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
        SEND_TO_CLIENT(RCRRP, UDS_A_TA_TYPE_PHYSICAL);

        // that remains unresolved at a time between p2 ms and p2 star ms
        POLL_FOR_MS(UDS_CLIENT_DEFAULT_P2_MS + 10);
        // the client should still be pending.
        ASSERT_INT_EQUAL(ctx.client.err, UDS_OK);
        ASSERT_INT_EQUAL(kRequestStateAwaitResponse, ctx.client.state)

        // When the client receives a positive response from the server
        const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
        SEND_TO_CLIENT(POSITIVE_RESPONSE, UDS_A_TA_TYPE_PHYSICAL);

        POLL_FOR_MS(5);

        // the client should return to the idle state with no error
        ASSERT_INT_EQUAL(kRequestStateIdle, ctx.client.state)
        ASSERT_INT_EQUAL(ctx.client.err, UDS_OK);
    }

    TEST_TEARDOWN();
}

void testClient0x34RequestDownload() {
    TEST_SETUP(CLIENT_ONLY);
    // When RequestDownload is called with these arguments
    ASSERT_INT_EQUAL(UDS_OK, UDSSendRequestDownload(&ctx.client, 0x11, 0x33, 0x602000, 0x00FFFF));

    // the bytes sent should match UDS-1 2013 Table 415
    const uint8_t CORRECT_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    ASSERT_CLIENT_SENT(CORRECT_REQUEST, UDS_A_TA_TYPE_PHYSICAL);
    TEST_TEARDOWN();
}

void testClient0x34UDSUnpackRequestDownloadResponse() {
    TEST_SETUP(CLIENT_ONLY);
    struct RequestDownloadResponse resp;

    // When the following raw bytes are received
    uint8_t RESPONSE[] = {0x74, 0x20, 0x00, 0x81};
    UDSClient_t client;
    memmove(client.recv_buf, RESPONSE, sizeof(RESPONSE));
    client.recv_size = sizeof(RESPONSE);

    UDSErr_t err = UDSUnpackRequestDownloadResponse(&client, &resp);

    // they should unpack without error
    ASSERT_INT_EQUAL(err, UDS_OK);
    ASSERT_INT_EQUAL(resp.maxNumberOfBlockLength, 0x81);
    TEST_TEARDOWN();
}

int main() {
    testServer0x10DiagSessCtrlIsDisabledByDefault();
    testServer0x10DiagSessCtrlFunctionalRequest();
    testServer0x11DoesNotSendOrReceiveMessagesAfterECUReset();
    testServer0x22RDBI1();
    testServer0x23ReadMemoryByAddress();
    testServer0x27SecurityAccess();
    testServer0x31RCRRP();
    testServer0x34NotEnabled();
    testServer0x34();
    testServer0x3ESuppressPositiveResponse();
    testServer0x83DiagnosticSessionControl();
    testServerSessionTimeout();

    testClientP2TimeoutExceeded();
    testClientP2TimeoutNotExceeded();
    testClientSuppressPositiveResponse();
    testClientBusy();
    testClient0x11ECUReset();
    testClient0x22RDBITxBufferTooSmall();
    testClient0x22RDBIUnpackResponse();
    testClient0x31RCRRP();
    testClient0x34RequestDownload();
    testClient0x34UDSUnpackRequestDownloadResponse();
}
