#include "test_iso14229.h"
#include "iso14229.h"
#include "iso14229client.h"
#include "iso14229server.h"
#include "isotp-c/isotp.h"
#include <assert.h>
#include <stdarg.h>

// ================================================
// Global Variables
// ================================================

uint32_t g_ms;
#define CAN_MESSAGE_QUEUE_SIZE 10
struct CANMessage g_serverRecvQueue[CAN_MESSAGE_QUEUE_SIZE];
int g_serverRecvQueueIdx = 0;
struct CANMessage g_clientRecvQueue[CAN_MESSAGE_QUEUE_SIZE];
int g_clientRecvQueueIdx = 0;

int g_serverSvcCallCount[ISO14229_MAX_DIAGNOSTIC_SERVICES] = {0};
Iso14229Service g_serverServices[ISO14229_MAX_DIAGNOSTIC_SERVICES] = {0};

// ================================================
// isotp-c Required Callback Functions
// ================================================

uint32_t isotp_user_get_ms() { return g_ms; }

void isotp_client_debug(const char *message, ...) {
    printf("CLIENT ISO-TP: ");
    va_list ap;
    va_start(ap, message);
    vprintf(message, ap);
    va_end(ap);
}

void isotp_server_phys_debug(const char *message, ...) {
    printf("SRV PHYS ISO-TP: ");
    va_list ap;
    va_start(ap, message);
    vprintf(message, ap);
    va_end(ap);
}

void isotp_server_func_debug(const char *message, ...) {
    printf("SRV FUNC ISO-TP: ");
    va_list ap;
    va_start(ap, message);
    vprintf(message, ap);
    va_end(ap);
}

// ================================================
// Iso14229Server Mock Functions
// ------------------------------------------------
// These are used in tests that require server callbacks
// ================================================

static enum Iso14229ResponseCode mockRdbiHandler(uint16_t data_id, uint8_t const **data_location,
                                                 uint16_t *len) {
    const uint8_t vin[] = {0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30, 0x34, 0x33,
                           0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
    const uint8_t data_0x010A[] = {0xA6, 0x66, 0x07, 0x50, 0x20, 0x1A,
                                   0x00, 0x63, 0x4A, 0x82, 0x7E};
    const uint8_t data_0x0110[] = {0x8C};
    switch (data_id) {
    case 0xF190:
        *data_location = vin;
        *len = sizeof(vin);
        break;
    case 0x010A:
        *data_location = data_0x010A;
        *len = sizeof(data_0x010A);
        break;
    case 0x0110:
        *data_location = data_0x0110;
        *len = sizeof(data_0x0110);
        break;
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}

static int g_mockECUResetHandlerCallCount = 0;
static enum Iso14229ResponseCode mockECUResetHandler(const struct Iso14229ServerStatus *status,
                                                     uint8_t resetType) {
    g_mockECUResetHandlerCallCount++;
    return kPositiveResponse;
}

static int g_mockSessionTimeoutHandlerCallCount = 0;
static void mockSessionTimeoutHandler() { g_mockSessionTimeoutHandlerCallCount++; }

// ================================================
// Fixtures
// ================================================

/**
 * @brief Sends CAN messages from client to an in-memory FIFO queue
 */
int fixtureClientSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    assert(size <= 8);
    assert(g_serverRecvQueueIdx < CAN_MESSAGE_QUEUE_SIZE);
    struct CANMessage *msg = &g_serverRecvQueue[g_serverRecvQueueIdx++];
    memmove(msg->data, data, size);
    msg->arbId = arbitration_id;
    msg->size = size;
    printf("c>0x%03x [%02d]: ", arbitration_id, g_serverRecvQueueIdx);
    PRINTHEX(data, size);
    return ISOTP_RET_OK;
}

/**
 * @brief Sends CAN messages from server to an in-memory FIFO queue
 */
int fixtureServerSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    assert(size <= 8);
    assert(g_clientRecvQueueIdx < CAN_MESSAGE_QUEUE_SIZE);
    struct CANMessage *msg = &g_clientRecvQueue[g_clientRecvQueueIdx++];
    memmove(msg->data, data, size);
    msg->arbId = arbitration_id;
    msg->size = size;
    printf("s>0x%03x [%02d]: ", arbitration_id, g_clientRecvQueueIdx);
    PRINTHEX(data, size);
    return ISOTP_RET_OK;
}

/**
 * @brief poll all IsoTpLinks and receive CAN from FIFO queues if available
 *
 * @param serverPhys
 * @param serverFunc
 */
static void fixtureIsoTpPollLinks(IsoTpLink *client, IsoTpLink *serverPhys, IsoTpLink *serverFunc) {
    assert(client);
    assert(serverPhys);
    assert(serverFunc);
    if (g_clientRecvQueueIdx > 0) {
        struct CANMessage *msg = &g_clientRecvQueue[0];
        if (msg->arbId == CLIENT_RECV_ID) {
            printf("c<0x%03x [%02d]: ", msg->arbId, g_clientRecvQueueIdx);
            PRINTHEX(msg->data, msg->size);
            isotp_on_can_message(client, msg->data, msg->size);
        }
        g_clientRecvQueueIdx--;
        memmove(g_clientRecvQueue, &g_clientRecvQueue[1], sizeof(struct CANMessage));
    }
    if (g_serverRecvQueueIdx > 0) {
        struct CANMessage *msg = &g_serverRecvQueue[0];
        if (msg->arbId == SERVER_PHYS_RECV_ID) {
            isotp_on_can_message(serverPhys, msg->data, msg->size);
        }
        if (msg->arbId == SERVER_FUNC_RECV_ID) {
            isotp_on_can_message(serverFunc, msg->data, msg->size);
        }
        g_serverRecvQueueIdx--;
        memmove(g_serverRecvQueue, &g_serverRecvQueue[1], sizeof(struct CANMessage));
    }
    isotp_poll(client);
    isotp_poll(serverPhys);
    isotp_poll(serverFunc);
}

/**
 * @brief A fixture for recording whether services were called
 *
 * @param srv
 * @param ctx
 * @return enum Iso14229ResponseCode
 */
enum Iso14229ResponseCode fixtureServiceHook(Iso14229Server *srv,
                                             const Iso14229ServerRequestContext *ctx) {
    assert(ctx);
    uint8_t sid = ctx->req.sid;
    printf("SID: %x called\n", sid);
    g_serverSvcCallCount[sid]++;
    return g_serverServices[sid](srv, ctx);
}

/**
 * @brief Install the Service Hook test fixture on the given server
 *
 * @param srv
 */
void fixtureInstallServerServiceHook(Iso14229Server *srv) {
    for (int i = 0; i < ISO14229_MAX_DIAGNOSTIC_SERVICES; i++) {
        g_serverServices[i] = srv->services[i];
        srv->services[i] = fixtureServiceHook;
    }
}

// ================================================
// Server tests
// ================================================

void testServerInit() {
    TEST_SETUP();
    SERVER_DECLARE();
    isotp_init_link(&serverPhysLink, CLIENT_RECV_ID, serverIsotpPhysSendBuf,
                    sizeof(serverIsotpPhysSendBuf), serverIsotpPhysRecvBuf,
                    sizeof(serverIsotpPhysRecvBuf), isotp_user_get_ms, fixtureServerSendCAN,
                    isotp_server_phys_debug);
    isotp_init_link(&serverFuncLink, CLIENT_RECV_ID, serverIsotpFuncSendBuf,
                    sizeof(serverIsotpFuncSendBuf), serverIsotpFuncRecvBuf,
                    sizeof(serverIsotpFuncRecvBuf), isotp_user_get_ms, fixtureServerSendCAN,
                    isotp_server_func_debug);
    Iso14229ServerInit(&server, &serverCfg);
    TEST_TEARDOWN();
}

/* services are disabled by default until iso14229ServerEnableService(...) is called */
void testServerDiagnosticSessionControlIsDisabledByDefault() {
    SERVER_TEST_SETUP();
    const uint8_t MOCK_DATA[] = {0x10, 0x02};
    const uint8_t CORRECT_RESPONSE[] = {0x7f, 0x10, 0x11};
    for (g_ms = 0; g_ms < server.cfg->p2_ms; g_ms++) {
        Iso14229ServerPoll(&server);
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        if (0 == g_ms) {
            isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA));
        }
    }
    ASSERT_MEMORY_EQUAL(CORRECT_RESPONSE, serverIsotpPhysSendBuf, sizeof(CORRECT_RESPONSE));
    TEST_TEARDOWN();
}

/* ISO14229-1 2013 Table 72 */
void testServerSuppressPositiveResponse() {
    SERVER_TEST_SETUP();
    const uint8_t MOCK_DATA[] = {0x3E, 0x80};
    iso14229ServerEnableService(&server, kSID_TESTER_PRESENT);
    for (g_ms = 0; g_ms < server.cfg->p2_ms; g_ms++) {
        Iso14229ServerPoll(&server);
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        if (0 == g_ms) {
            isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA));
        }
    }
    ASSERT_EQUAL(clientLink.receive_size, 0);
    TEST_TEARDOWN();
}

// Special-case of ECU reset service
// ISO-14229-1 2013 9.3.1:
// on the behaviour of the ECU from the time following the positive response message to the ECU
// reset request: It is recommended that during this time the ECU does not accept any request
// messages and send any response messages.
void testServerDoesNotSendOrReceiveMessagesAfterECUReset() {
    SERVER_TEST_SETUP();
    uint16_t size = 0;
    const uint8_t MOCK_DATA[] = {0x11, 0x01};
    iso14229ServerEnableService(&server, kSID_ECU_RESET);
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA)); // first request
    step = 1;
    break;
case 1: {
    int ret =
        isotp_receive(&clientLink, clientLink.receive_buffer, clientLink.receive_buf_size, &size);
    if (ISOTP_RET_OK == ret) {
        ASSERT_EQUAL(size, 3);
        isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA)); // duplicate request
        step = 2;
    }
    break;
}
case 2: {
    if (g_mockECUResetHandlerCallCount) {
        done = true;
    }
    break;
}
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 3);
    int ret =
        isotp_receive(&clientLink, clientLink.receive_buffer, clientLink.receive_buf_size, &size);
    ASSERT_EQUAL(ret, ISOTP_RET_NO_DATA);
    TEST_TEARDOWN();
}

enum Iso14229ResponseCode
fixtureTestServerRCRRPStartRoutine(const struct Iso14229ServerStatus *status, void *userCtx,
                                   Iso14229RoutineControlArgs *args) {
    static bool userFlag = false;
    if (userFlag) {
        /* a call to `doLongBlockingTask();` would go here */
        return kPositiveResponse;
    } else {
        userFlag = true;
        return kRequestCorrectlyReceived_ResponsePending;
    }
}

// ISO-14229-1 2013 Table A.1 Byte Value 0x78: requestCorrectlyReceived-ResponsePending
// "This NRC is in general supported by each diagnostic service".
void testServerRCRRP() {
    SERVER_TEST_SETUP();
    Iso14229Routine routine = {
        .routineIdentifier = 0x1234,
        .startRoutine = fixtureTestServerRCRRPStartRoutine,
        .stopRoutine = NULL,
        .requestRoutineResults = NULL,
        .userCtx = NULL,
    };
    Iso14229ServerRegisterRoutine(&server, &routine);
    const uint8_t MOCK_DATA[] = {0x31, 0x01, 0x12, 0x34};
    iso14229ServerEnableService(&server, kSID_ROUTINE_CONTROL);
    uint16_t size = 0;
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA));
    step = 1;
    break;
case 1: {
    const uint8_t EXPECTED_RESPONSE[] = {0x7F, 0x31, 0x78};
    int ret =
        isotp_receive(&clientLink, clientLink.receive_buffer, clientLink.receive_buf_size, &size);
    if (ISOTP_RET_OK == ret) {
        ASSERT_EQUAL(sizeof(EXPECTED_RESPONSE), size);
        ASSERT_MEMORY_EQUAL(clientLink.receive_buffer, EXPECTED_RESPONSE,
                            sizeof(EXPECTED_RESPONSE));
        step = 2;
    }
    break;
}
case 2: {
    const uint8_t EXPECTED_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
    int ret =
        isotp_receive(&clientLink, clientLink.receive_buffer, clientLink.receive_buf_size, &size);
    if (ISOTP_RET_OK == ret) {
        PRINTHEX(clientLink.receive_buffer, size);
        ASSERT_EQUAL(sizeof(EXPECTED_RESPONSE), size);
        ASSERT_MEMORY_EQUAL(clientLink.receive_buffer, EXPECTED_RESPONSE,
                            sizeof(EXPECTED_RESPONSE));
        done = true;
    }
    break;
}
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 5);
    TEST_TEARDOWN();
}

// ================================================
// Client tests
// ================================================

void testClientInit() {
    TEST_SETUP();
    CLIENT_DECLARE();
    isotp_init_link(&clientLink, CLIENT_SEND_ID, clientIsotpSendBuf, sizeof(clientIsotpSendBuf),
                    clientIsotpRecvBuf, sizeof(clientIsotpRecvBuf), isotp_user_get_ms,
                    fixtureClientSendCAN, isotp_client_debug);
    iso14229ClientInit(&client, &clientCfg);
    TEST_TEARDOWN();
}

void testClientP2TimeoutExceeded() {
    CLIENT_TEST_SETUP();
    for (g_ms = 0; g_ms < client.settings.p2_ms + 3; g_ms++) {
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ASSERT_EQUAL(kRequestNoError, ECUReset(&client, kHardReset));
        }
    }
    ASSERT_EQUAL(kRequestTimedOut, client.ctx.err);
    ASSERT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientP2TimeoutNotExceeded() {
    CLIENT_TEST_SETUP();
    static uint8_t response[] = {0x51, 0x01};
    for (g_ms = 0; g_ms < client.settings.p2_ms + 2; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        } else if (1 == g_ms) {
            isotp_send(&serverPhysLink, response, sizeof(response));
        }
    }
    ASSERT_EQUAL(kRequestNoError, client.ctx.err);
    ASSERT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientECUReset() {
    CLIENT_TEST_SETUP();
    const uint8_t REQUEST[] = {0x11, 0x01};  // correct
    const uint8_t RESPONSE[] = {0x51, 0x01}; // correct, positive
    for (g_ms = 0; g_ms < client.settings.p2_ms + 2; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
            ASSERT_MEMORY_EQUAL(clientIsotpSendBuf, REQUEST, sizeof(REQUEST));
        } else if (1 == g_ms) {
            isotp_send(&serverPhysLink, RESPONSE, sizeof(RESPONSE));
        }
    }
    ASSERT_EQUAL(kRequestNoError, client.ctx.err);
    ASSERT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientECUResetNegativeResponse() {
    CLIENT_TEST_SETUP();
    static uint8_t RESPONSE[] = {0x7F, 0x51, 0x01}; // correct, negative
    for (g_ms = 0; g_ms < 4; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        } else if (1 == g_ms) {
            isotp_send(&serverPhysLink, RESPONSE, sizeof(RESPONSE));
        }
    }
    ASSERT_EQUAL(kRequestErrorNegativeResponse, client.ctx.err);
    ASSERT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientSuppressPositiveResponse() {
    CLIENT_TEST_SETUP();
    client.settings.suppressPositiveResponse = true;
    for (g_ms = 0; g_ms < client.settings.p2_ms + 2; g_ms++) {
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ASSERT_EQUAL(kRequestNoError, ECUReset(&client, kHardReset));
        }
    }
    ASSERT_EQUAL(kRequestNoError, client.ctx.err);
    ASSERT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientBusy() {
    CLIENT_TEST_SETUP();
    for (g_ms = 0; g_ms < 2; g_ms++) {
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        } else if (1 == g_ms) {
            ASSERT_EQUAL(kRequestNotSentBusy, ECUReset(&client, kHardReset));
        }
    }
    TEST_TEARDOWN();
}

void testClientUnexpectedResponse() {
    CLIENT_TEST_SETUP();
    // The correct response SID to EcuReset (0x11) is 0x51.
    const uint8_t RESPONSE[] = {0x50, 0x01}; // incorrect, unexpected
    for (g_ms = 0; g_ms < 4; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        } else if (1 == g_ms) {
            isotp_send(&serverPhysLink, RESPONSE, sizeof(RESPONSE));
        }
    }
    ASSERT_EQUAL(kRequestErrorResponseSIDMismatch, client.ctx.err);
    ASSERT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientTransferData() {
    CLIENT_TEST_SETUP();
    RequestDownload(&client, 0x11, 0x33, 0x602000, 0x00FFFF);
    const uint8_t CORRECT[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    ASSERT_MEMORY_EQUAL(clientIsotpSendBuf, CORRECT, sizeof(CORRECT));
    TEST_TEARDOWN();
}

/* ISO14229-1 2013 14.5.5 */
void testClientTransferData2() {
    CLIENT_TEST_SETUP();

#define MemorySize (0x00FFFF)
    uint8_t SRC_DATA[MemorySize] = {0};
    for (int i = 0; i < sizeof(SRC_DATA); i++) {
        SRC_DATA[i] = i & 0xFF;
    }
    FILE *fd = fmemopen(SRC_DATA, sizeof(SRC_DATA), "r");

    const uint16_t maximumNumberOfBlockLength = 0x0081;
    uint32_t blockNr = 0;

    CLIENT_TEST_SEQUENCE_BEGIN();
case 0:
    RequestDownload(&client, 0x11, 0x33, 0x602000, MemorySize);
    step = 1;
    break;

case 1: {
    uint16_t size = 0;
    if (ISOTP_RET_OK == isotp_receive(&serverPhysLink, serverIsotpPhysRecvBuf,
                                      sizeof(serverIsotpPhysRecvBuf), &size)) {
        // Table 415
        ASSERT_EQUAL(size, 9);
        const uint8_t CORRECT_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
        ASSERT_MEMORY_EQUAL(serverIsotpPhysRecvBuf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));

        const uint8_t RESPONSE[] = {0x74, 0x20, 0x00, 0x81};
        isotp_send(&serverPhysLink, RESPONSE, sizeof(RESPONSE));
        step = 2;
    }
    break;
}

case 2: { // Transfer Data
    assert(!ferror(fd));
    if (!feof(fd)) {
        if (kRequestStateIdle == client.ctx.state) {
            TransferData(&client, ++blockNr & 0xFF, maximumNumberOfBlockLength, fd);

            const uint8_t RESPONSE[] = {0x76, blockNr};
            isotp_send(&serverPhysLink, RESPONSE, sizeof(RESPONSE));
        }
    } else {
        // The last message contains the final three bytes of the source data
        uint8_t REQUEST[5] = {0x36, 0x05, 0x0, 0x0, 0x0};
        for (int i = 0; i < 3; i++) {
            REQUEST[2 + i] = SRC_DATA[(MemorySize - 1) - 2 + i];
        }
        ASSERT_EQUAL(serverPhysLink.receive_size, sizeof(REQUEST));
        ASSERT_MEMORY_EQUAL(serverIsotpPhysRecvBuf, REQUEST, sizeof(REQUEST)); // Table 419

        ASSERT_EQUAL(blockNr, 517); // 14.5.5.1.1
        step = 3;                   // Transfer complete
    }
    break;
}

case 3: // Request Transfer Exit
    if (kRequestStateIdle == client.ctx.state) {
        ASSERT_EQUAL(client.ctx.err, kRequestNoError);
        RequestTransferExit(&client);
        step = 4;
    }
    break;
case 4: {
    uint16_t size = 0;
    if (ISOTP_RET_OK == isotp_receive(&serverPhysLink, serverIsotpPhysRecvBuf,
                                      sizeof(serverIsotpPhysRecvBuf), &size)) {
        PRINTHEX(serverIsotpPhysRecvBuf, size);
        ASSERT_EQUAL(size, 1);
        const uint8_t CORRECT_REQUEST[] = {0x37};
        ASSERT_MEMORY_EQUAL(serverIsotpPhysRecvBuf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));
        done = true;
    }
    break;
}
    CLIENT_TEST_SEQUENCE_END(60000);
#undef MemorySize
    TEST_TEARDOWN();
}

void testClientRequestCorrectlyReceivedResponsePending() {
    CLIENT_TEST_SETUP();
    client.settings.p2_ms = 10;
    client.settings.p2_star_ms = 50;
    const uint8_t RESPONSE1[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
    const uint8_t RESPONSE2[] = {0x71, 0x01, 0x12, 0x34}; // PositiveResponse
    uint32_t t_1 = 0;
    CLIENT_TEST_SEQUENCE_BEGIN();
case 0: {
    RoutineControl(&client, kStartRoutine, 0x1234, NULL, 0);
    step = 1;
    break;
}
case 1: {
    isotp_send(&serverPhysLink, RESPONSE1, sizeof(RESPONSE1));
    step = 2;
    // At this time, the client should be waiting with timeout p2_star
    t_1 = g_ms + 20;
    break;
}
case 2: {
    if (Iso14229TimeAfter(t_1, g_ms)) {
        ASSERT_EQUAL(client.ctx.state, kRequestStateSentAwaitResponse);
        isotp_send(&serverPhysLink, RESPONSE2, sizeof(RESPONSE2));
        step = 3;
    }
    break;
}
case 3: {
    if (kRequestStateIdle == client.ctx.state) {
        done = true;
    }
    break;
}
    CLIENT_TEST_SEQUENCE_END(1000);
    ASSERT_EQUAL(client.ctx.err, kRequestNoError);
    TEST_TEARDOWN();
}

// ================================================
// System tests
// ================================================

void testSystemEcuReset() {
    CLIENT_SERVER_TEST_SETUP();
    iso14229ServerEnableService(&server, kSID_ECU_RESET);
    fixtureInstallServerServiceHook(&server);

    for (g_ms = 0; g_ms < client.settings.p2_ms + 2; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        Iso14229ServerPoll(&server);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        }
    }

    ASSERT_EQUAL(g_serverSvcCallCount[kSID_ECU_RESET], 1);
    TEST_TEARDOWN();
}

/**
 * @brief run all tests
 */
int main() {
    testServerInit();
    testServerDiagnosticSessionControlIsDisabledByDefault();
    testServerSuppressPositiveResponse();
    testServerDoesNotSendOrReceiveMessagesAfterECUReset();
    testServerRCRRP();

    testClientInit();
    testClientECUReset();
    testClientECUResetNegativeResponse();
    testClientP2TimeoutExceeded();
    testClientP2TimeoutNotExceeded();
    testClientSuppressPositiveResponse();
    testClientBusy();
    testClientUnexpectedResponse();
    testClientTransferData();
    testClientTransferData2();
    testClientRequestCorrectlyReceivedResponsePending();

    testSystemEcuReset();
}
