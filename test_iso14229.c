#include "test_iso14229.h"
#include "iso14229.h"
#include "iso14229client.h"
#include "iso14229server.h"
#include "isotp-c/isotp.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

// ================================================
// Global Variables
// ================================================

int g_ms;
#define CAN_MESSAGE_QUEUE_SIZE 10
struct CANMessage g_serverRecvQueue[CAN_MESSAGE_QUEUE_SIZE];
int g_serverRecvQueueIdx = 0;
struct CANMessage g_clientRecvQueue[CAN_MESSAGE_QUEUE_SIZE];
int g_clientRecvQueueIdx = 0;

int g_serverSvcCallCount[kISO14229_SID_NOT_SUPPORTED] = {0};
Iso14229Service g_serverServices[kISO14229_SID_NOT_SUPPORTED] = {0};

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

static enum Iso14229ResponseCode
mockDiagnosticSessionControlHandler(const struct Iso14229ServerStatus *status,
                                    enum Iso14229DiagnosticSessionType type) {
    (void)status;
    (void)type;
    return kPositiveResponse;
}

static enum Iso14229ResponseCode mockUserRdbiHandler(const struct Iso14229ServerStatus *status,
                                                     uint16_t data_id,
                                                     uint8_t const **data_location, uint16_t *len) {
    (void)status;
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
                                                     uint8_t resetType, uint8_t *powerDownTime) {
    (void)status;
    (void)resetType;
    (void)powerDownTime;
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
    ISO14229USERDEBUG("c>0x%03x [%02d]: ", arbitration_id, g_serverRecvQueueIdx);
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
    ISO14229USERDEBUG("s>0x%03x [%02d]: ", arbitration_id, g_clientRecvQueueIdx);
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
            ISO14229USERDEBUG("c<0x%03x [%02d]: ", msg->arbId, g_clientRecvQueueIdx);
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

void testServer0x10DiagnosticSessionControlIsDisabledByDefault() {
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

// Special-case of ECU reset service
// ISO-14229-1 2013 9.3.1:
// on the behaviour of the ECU from the time following the positive response message to the ECU
// reset request: It is recommended that during this time the ECU does not accept any request
// messages and send any response messages.
void testServer0x11DoesNotSendOrReceiveMessagesAfterECUReset() {
    SERVER_TEST_SETUP();
    serverCfg.userECUResetHandler = mockECUResetHandler;
    uint16_t size = 0;
    const uint8_t MOCK_DATA[] = {0x11, 0x01};
    const uint8_t EXPECTED_RESPONSE[] = {0x51, 0x01};
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    SERVER_TEST_CLIENT_SEND(MOCK_DATA);
    break;
case 1:
    SERVER_TEST_AWAIT_RESPONSE(EXPECTED_RESPONSE);
    break;
case 2:
    SERVER_TEST_CLIENT_SEND(MOCK_DATA);
    break;
case 3: {
    if (g_mockECUResetHandlerCallCount) {
        done = true;
    }
    break;
}
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 3);
    int ret =
        isotp_receive(&clientLink, clientLink.receive_buffer, clientLink.receive_buf_size, &size);
    ASSERT_INT_EQUAL(ret, ISOTP_RET_NO_DATA);
    TEST_TEARDOWN();
}

void testServer0x22RDBI1() {
    SERVER_TEST_SETUP();
    serverCfg.userRDBIHandler = mockUserRdbiHandler;
    const uint8_t MOCK_DATA[] = {0x22, 0xF1, 0x90};
    const uint8_t CORRECT_RESPONSE[] = {0x62, 0xF1, 0x90, 0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30,
                                        0x34, 0x33, 0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
    for (g_ms = 0; g_ms < server.cfg->p2_ms; g_ms++) {
        Iso14229ServerPoll(&server);
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        if (0 == g_ms) {
            isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA));
        }
    }
    ASSERT_INT_EQUAL(sizeof(CORRECT_RESPONSE), serverPhysLink.send_size);
    ASSERT_MEMORY_EQUAL(CORRECT_RESPONSE, serverIsotpPhysSendBuf, sizeof(CORRECT_RESPONSE));
    TEST_TEARDOWN();
}

enum Iso14229ResponseCode mockSecurityAccessGenerateSeed(const struct Iso14229ServerStatus *status,
                                                         uint8_t level, const uint8_t *in_data,
                                                         uint16_t in_size, uint8_t *out_data,
                                                         uint16_t out_bufsize, uint16_t *out_size) {
    const uint8_t seed[] = {0x36, 0x57};
    (void)status;
    (void)level;
    (void)in_data;
    (void)in_size;
    if (status->securityLevel == level) {
        assert(out_bufsize >= 2);
        out_data[0] = 0;
        out_data[1] = 0;
        *out_size = 2;
    } else {
        assert(out_bufsize >= sizeof(seed));
        memmove(out_data, seed, sizeof(seed));
        *out_size = sizeof(seed);
    }
    return kPositiveResponse;
}

enum Iso14229ResponseCode mockSecurityAccessValidateKey(const struct Iso14229ServerStatus *status,
                                                        uint8_t level, const uint8_t *key,
                                                        uint16_t size) {
    (void)status;
    (void)level;
    (void)key;
    (void)size;
    return kPositiveResponse;
}

// ISO14229-1 2013 9.4.5.2
void testServer0x27SecurityAccess() {
    SERVER_TEST_SETUP();
    serverCfg.userSecurityAccessGenerateSeed = mockSecurityAccessGenerateSeed;
    serverCfg.userSecurityAccessValidateKey = mockSecurityAccessValidateKey;
    const uint8_t MOCK_DATA_1[] = {0x27, 0x01};
    const uint8_t EXPECTED_RESPONSE_1[] = {0x67, 0x01, 0x36, 0x57};
    const uint8_t MOCK_DATA_2[] = {0x27, 0x02, 0xC9, 0xA9};
    const uint8_t EXPECTED_RESPONSE_2[] = {0x67, 0x02};
    ASSERT_INT_EQUAL(server.status.securityLevel, 0);
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    SERVER_TEST_CLIENT_SEND(MOCK_DATA_1);
    break;
case 1:
    SERVER_TEST_AWAIT_RESPONSE(EXPECTED_RESPONSE_1);
    break;
case 2:
    SERVER_TEST_CLIENT_SEND(MOCK_DATA_2);
    break;
case 3:
    SERVER_TEST_AWAIT_RESPONSE(EXPECTED_RESPONSE_2);
    break;
case 4:
    ASSERT_INT_EQUAL(server.status.securityLevel, 1);
    done = true;
    break;
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 5);
    TEST_TEARDOWN();
}

// ISO14229-1 2013 9.4.5.3
void testServer0x27SecurityAccessAlreadyUnlocked() {
    SERVER_TEST_SETUP();
    serverCfg.userSecurityAccessGenerateSeed = mockSecurityAccessGenerateSeed;
    serverCfg.userSecurityAccessValidateKey = mockSecurityAccessValidateKey;
    server.status.securityLevel = 1;
    const uint8_t MOCK_DATA_1[] = {0x27, 0x01};
    const uint8_t EXPECTED_RESPONSE_1[] = {0x67, 0x01, 0x00, 0x00};
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    SERVER_TEST_CLIENT_SEND(MOCK_DATA_1);
    break;
case 1:
    SERVER_TEST_AWAIT_RESPONSE(EXPECTED_RESPONSE_1);
    break;
case 2:
    done = true;
    ASSERT_INT_EQUAL(server.status.securityLevel, 1);
    break;
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 5);
    TEST_TEARDOWN();
}

static enum Iso14229ResponseCode testServer0x31RCRRPMockRoutineControl(
    const struct Iso14229ServerStatus *status, enum RoutineControlType routineControlType,
    uint16_t routineIdentifier, Iso14229RoutineControlArgs *args) {
    (void)status;
    (void)routineControlType;
    (void)args;
    static bool userFlag = false;
    ASSERT_INT_EQUAL(0x1234, routineIdentifier);
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
void testServer0x31RCRRP() {
    SERVER_TEST_SETUP();
    serverCfg.userRoutineControlHandler = testServer0x31RCRRPMockRoutineControl;
    const uint8_t MOCK_DATA[] = {0x31, 0x01, 0x12, 0x34};
    const uint8_t EXPECTED_RESPONSE_1[] = {0x7F, 0x31, 0x78};
    const uint8_t EXPECTED_RESPONSE_2[] = {0x71, 0x01, 0x12, 0x34};
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    SERVER_TEST_CLIENT_SEND(MOCK_DATA);
    break;
case 1:
    SERVER_TEST_AWAIT_RESPONSE(EXPECTED_RESPONSE_1);
    break;
case 2:
    SERVER_TEST_AWAIT_RESPONSE(EXPECTED_RESPONSE_2);
    break;
case 3:
    done = true;
    break;
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 5);
    TEST_TEARDOWN();
}

void testServer0x34NotEnabled() {
    SERVER_TEST_SETUP();
    const uint8_t REQUEST_DOWNLOAD_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20,
                                                0x00, 0x00, 0xFF, 0xFF};
    const uint8_t REQUEST_DOWNLOAD_RESPONSE[] = {0x7F, 0x34, 0x11};
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    SERVER_TEST_CLIENT_SEND(REQUEST_DOWNLOAD_REQUEST);
    break;
case 1:
    SERVER_TEST_AWAIT_RESPONSE(REQUEST_DOWNLOAD_RESPONSE);
    break;
case 2:
    done = true;
    break;
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 3);
    TEST_TEARDOWN();
}

static enum Iso14229ResponseCode testServer0x34DownloadDataMockHandlerOnExit(
    const struct Iso14229ServerStatus *status, void *userCtx, uint16_t buffer_size,
    uint8_t *transferResponseParameterRecord, uint16_t *transferResponseParameterRecordSize) {
    (void)status;
    (void)userCtx;
    (void)buffer_size;
    (void)transferResponseParameterRecord;
    (void)transferResponseParameterRecordSize;
    return kPositiveResponse;
}

static enum Iso14229ResponseCode
testServer0x34DownloadDataMockHandlerOnTransfer(const struct Iso14229ServerStatus *status,
                                                void *userCtx, const uint8_t *data, uint32_t len) {
    (void)status;
    (void)userCtx;
    (void)data;
    (void)len;
    return kPositiveResponse;
}

Iso14229DownloadHandler testServer0x34DownloadDataMockHandler = {
    .onExit = testServer0x34DownloadDataMockHandlerOnExit,
    .onTransfer = testServer0x34DownloadDataMockHandlerOnTransfer,
    .userCtx = NULL,
};

static enum Iso14229ResponseCode testServer0x34DownloadDataMockuserRequestDownloadHandler(
    const struct Iso14229ServerStatus *status, void *memoryAddress, size_t memorySize,
    uint8_t dataFormatIdentifier, Iso14229DownloadHandler **handler,
    uint16_t *maxNumberOfBlockLength) {
    (void)status;
    ASSERT_INT_EQUAL(0x11, dataFormatIdentifier);
    ASSERT_PTR_EQUAL((void *)0x602000, memoryAddress);
    ASSERT_INT_EQUAL(0x00FFFF, memorySize);
    *handler = &testServer0x34DownloadDataMockHandler;
    *maxNumberOfBlockLength = 0x0081;
    return kPositiveResponse;
}

void testServer0x34DownloadData() {
    SERVER_TEST_SETUP();
    serverCfg.userRequestDownloadHandler = testServer0x34DownloadDataMockuserRequestDownloadHandler;
    // ISO14229-1:2013 Table 415
    const uint8_t REQUEST_DOWNLOAD_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20,
                                                0x00, 0x00, 0xFF, 0xFF};
    const uint8_t REQUEST_DOWNLOAD_RESPONSE[] = {0x74, 0x20, 0x00, 0x81};
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    SERVER_TEST_CLIENT_SEND(REQUEST_DOWNLOAD_REQUEST);
    break;
case 1:
    SERVER_TEST_AWAIT_RESPONSE(REQUEST_DOWNLOAD_RESPONSE);
    break;
case 2:
    done = true;
    break;
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 3);
    TEST_TEARDOWN();
}

#define TEST_0x36_MOCK_DATA 0xF0, 0x00, 0xBA, 0xBA
static enum Iso14229ResponseCode
testServer0x36TransferDataMockHandlerOnTransfer(const struct Iso14229ServerStatus *status,
                                                void *userCtx, const uint8_t *data, uint32_t len) {
    (void)status;
    (void)userCtx;
    const uint8_t MOCK_DATA[] = {TEST_0x36_MOCK_DATA};
    ASSERT_INT_EQUAL(sizeof(MOCK_DATA), len);
    ASSERT_MEMORY_EQUAL(MOCK_DATA, data, len);
    return kPositiveResponse;
}

Iso14229DownloadHandler testServer0x36TransferDataMockHandler = {
    .onTransfer = testServer0x36TransferDataMockHandlerOnTransfer,
};

void testServer0x36TransferData() {
    SERVER_TEST_SETUP();
    server.downloadHandler = &testServer0x36TransferDataMockHandler;
    Iso14229DownloadHandlerInit(server.downloadHandler, 0xFFFF);
    const uint8_t TRANSFER_DATA_REQUEST[] = {0x36, 0x01, TEST_0x36_MOCK_DATA};
#undef TEST_0x36_MOCK_DATA
    const uint8_t TRANSFER_DATA_RESPONSE[] = {0x76, 0x01};
    SERVER_TEST_SEQUENCE_BEGIN();
case 0:
    SERVER_TEST_CLIENT_SEND(TRANSFER_DATA_REQUEST);
    break;
case 1:
    SERVER_TEST_AWAIT_RESPONSE(TRANSFER_DATA_RESPONSE);
    break;
case 2:
    done = true;
    break;
    SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 3);
    ASSERT_INT_EQUAL(server.downloadHandler->numBytesTransferred, 4);
    ASSERT_INT_EQUAL(server.downloadHandler->blockSequenceCounter, 2);
    TEST_TEARDOWN();
}

/* ISO14229-1 2013 Table 72 */
void testServer0x3ESuppressPositiveResponse() {
    SERVER_TEST_SETUP();
    const uint8_t MOCK_DATA[] = {0x3E, 0x80};
    for (g_ms = 0; g_ms < server.cfg->p2_ms; g_ms++) {
        Iso14229ServerPoll(&server);
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        if (0 == g_ms) {
            isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA));
        }
    }
    ASSERT_INT_EQUAL(clientLink.receive_size, 0);
    TEST_TEARDOWN();
}

void testServer0x83DiagnosticSessionControl() {
    SERVER_TEST_SETUP();
    serverCfg.userDiagnosticSessionControlHandler = mockDiagnosticSessionControlHandler;
    const uint8_t MOCK_DATA[] = {0x10, 0x83};
    ASSERT_INT_EQUAL(server.status.sessionType, kDefaultSession);
    for (g_ms = 0; g_ms < server.cfg->p2_ms; g_ms++) {
        Iso14229ServerPoll(&server);
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        if (0 == g_ms) {
            isotp_send(&clientLink, MOCK_DATA, sizeof(MOCK_DATA));
        }
    }
    ASSERT_INT_EQUAL(server.status.sessionType, kExtendedDiagnostic);
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
            ASSERT_INT_EQUAL(kRequestNoError, ECUReset(&client, kHardReset));
        }
    }
    ASSERT_INT_EQUAL(kRequestTimedOut, client.ctx.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.ctx.state);
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
    ASSERT_INT_EQUAL(kRequestNoError, client.ctx.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientSuppressPositiveResponse() {
    CLIENT_TEST_SETUP();
    client.settings.suppressPositiveResponse = true;
    for (g_ms = 0; g_ms < client.settings.p2_ms + 2; g_ms++) {
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ASSERT_INT_EQUAL(kRequestNoError, ECUReset(&client, kHardReset));
        }
    }
    ASSERT_INT_EQUAL(kRequestNoError, client.ctx.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClientBusy() {
    CLIENT_TEST_SETUP();
    for (g_ms = 0; g_ms < 2; g_ms++) {
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        } else if (1 == g_ms) {
            ASSERT_INT_EQUAL(kRequestNotSentBusy, ECUReset(&client, kHardReset));
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
    ASSERT_INT_EQUAL(kRequestErrorResponseSIDMismatch, client.ctx.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClient0x11ECUReset() {
    CLIENT_TEST_SETUP();
    const uint8_t REQUEST[] = {0x11, 0x01};  // correct
    const uint8_t RESPONSE[] = {0x51, 0x01}; // correct, positive
    for (g_ms = 0; g_ms < client.settings.p2_ms + 2; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
            ASSERT_MEMORY_EQUAL(clientIsotpSendBuf, REQUEST, sizeof(REQUEST));
            assert(sizeof(REQUEST) == clientLink.send_size);
        } else if (1 == g_ms) {
            isotp_send(&serverPhysLink, RESPONSE, sizeof(RESPONSE));
        }
    }
    ASSERT_INT_EQUAL(kRequestNoError, client.ctx.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClient0x11ECUResetNegativeResponse() {
    CLIENT_TEST_SETUP();
    static uint8_t RESPONSE[] = {0x7F, 0x11, 0x10}; // ECU Reset Negative Response
    for (g_ms = 0; g_ms < 4; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        } else if (1 == g_ms) {
            isotp_send(&serverPhysLink, RESPONSE, sizeof(RESPONSE));
        }
    }
    ASSERT_INT_EQUAL(kRequestErrorNegativeResponse, client.ctx.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.ctx.state);
    TEST_TEARDOWN();
}

void testClient0x22RDBITxBufferTooSmall() {
    CLIENT_TEST_SETUP();
    uint16_t didList[] = {0x0001, 0x0002, 0x0003};
    client.cfg->link->send_buf_size = 4;
    ASSERT_INT_EQUAL(kRequestNotSentInvalidArgs,
                     ReadDataByIdentifier(&client, didList, ARRAY_SZ(didList)))
    TEST_TEARDOWN();
}

void testClient0x22RDBIUnpackResponse() {
    CLIENT_TEST_SETUP();
    uint8_t RESPONSE[] = {0x72, 0x12, 0x34, 0x00, 0x00, 0xAA, 0x00, 0x56, 0x78, 0xAA, 0xBB};
    struct Iso14229Response resp = {
        .buf = RESPONSE, .buffer_size = sizeof(RESPONSE), .len = sizeof(RESPONSE)};
    uint8_t buf[4];
    uint16_t offset = 0;
    int err = 0;
    err = RDBIReadDID(&resp, 0x1234, buf, 4, &offset);
    ASSERT_INT_EQUAL(err, READ_DID_NO_ERR);
    uint32_t d0 = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
    ASSERT_INT_EQUAL(d0, 0x0000AA00);
    err = RDBIReadDID(&resp, 0x1234, buf, 2, &offset);
    ASSERT_INT_EQUAL(err, READ_DID_ERR_DID_MISMATCH);
    err = RDBIReadDID(&resp, 0x5678, buf, 20, &offset);
    ASSERT_INT_EQUAL(err, READ_DID_ERR_RESPONSE_TOO_SHORT);
    err = RDBIReadDID(&resp, 0x5678, buf, 2, &offset);
    ASSERT_INT_EQUAL(err, READ_DID_NO_ERR);
    uint16_t d1 = (buf[0] << 8) + buf[1];
    ASSERT_INT_EQUAL(d1, 0xAABB);
    err = RDBIReadDID(&resp, 0x5678, buf, 1, &offset);
    ASSERT_INT_EQUAL(err, READ_DID_ERR_RESPONSE_TOO_SHORT);
    ASSERT_INT_EQUAL(offset, sizeof(RESPONSE));
    TEST_TEARDOWN();
}

void testClient0x31RequestCorrectlyReceivedResponsePending() {
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
        ASSERT_INT_EQUAL(client.ctx.state, kRequestStateSentAwaitResponse);
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
    ASSERT_INT_EQUAL(client.ctx.err, kRequestNoError);
    TEST_TEARDOWN();
}

// ISO14229-1 2013 Table 415
void testClient0x34RequestDownload() {
    CLIENT_TEST_SETUP();
    const uint8_t REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    ASSERT_INT_EQUAL(kRequestNoError, RequestDownload(&client, 0x11, 0x33, 0x602000, 0x00FFFF));
    ASSERT_MEMORY_EQUAL(clientIsotpSendBuf, REQUEST, sizeof(REQUEST));
    ASSERT_INT_EQUAL(sizeof(REQUEST), clientLink.send_size);
    TEST_TEARDOWN();
}

void testClient0x34UnpackRequestDownloadResponse() {
    uint8_t RESPONSE[] = {0x74, 0x20, 0x00, 0x81};
    struct Iso14229Response resp;
    resp.buf = RESPONSE;
    resp.buffer_size = sizeof(RESPONSE);
    resp.len = sizeof(RESPONSE);
    struct RequestDownloadResponse unpacked;
    enum Iso14229ClientRequestError err = UnpackRequestDownloadResponse(&resp, &unpacked);
    ASSERT_INT_EQUAL(unpacked.maxNumberOfBlockLength, 0x81);
    ASSERT_INT_EQUAL(err, kRequestNoError);
}

void testClient0x36TransferData() {
    CLIENT_TEST_SETUP();
    RequestDownload(&client, 0x11, 0x33, 0x602000, 0x00FFFF);
    const uint8_t CORRECT[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    ASSERT_MEMORY_EQUAL(clientIsotpSendBuf, CORRECT, sizeof(CORRECT));
    TEST_TEARDOWN();
}

/* ISO14229-1 2013 14.5.5 */
void testClient0x36TransferData2() {
    CLIENT_TEST_SETUP();

#define MemorySize (0x00FFFF)
    const char *fname = "testClient0x36TransferData2.dat";
    uint8_t SRC_DATA[MemorySize] = {0};
    for (unsigned int i = 0; i < sizeof(SRC_DATA); i++) {
        SRC_DATA[i] = i & 0xFF;
    }

    FILE *fd = fopen(fname, "wb+");
    assert(sizeof(SRC_DATA) == fwrite(SRC_DATA, 1, sizeof(SRC_DATA), fd));
    rewind(fd);

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
        ASSERT_INT_EQUAL(size, 9);
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
        ASSERT_INT_EQUAL(serverPhysLink.receive_size, sizeof(REQUEST));
        ASSERT_MEMORY_EQUAL(serverIsotpPhysRecvBuf, REQUEST, sizeof(REQUEST)); // Table 419

        ASSERT_INT_EQUAL(blockNr, 517); // 14.5.5.1.1
        step = 3;                       // Transfer complete
    }
    break;
}

case 3: // Request Transfer Exit
    if (kRequestStateIdle == client.ctx.state) {
        ASSERT_INT_EQUAL(client.ctx.err, kRequestNoError);
        RequestTransferExit(&client);
        step = 4;
    }
    break;
case 4: {
    uint16_t size = 0;
    if (ISOTP_RET_OK == isotp_receive(&serverPhysLink, serverIsotpPhysRecvBuf,
                                      sizeof(serverIsotpPhysRecvBuf), &size)) {
        PRINTHEX(serverIsotpPhysRecvBuf, size);
        ASSERT_INT_EQUAL(size, 1);
        const uint8_t CORRECT_REQUEST[] = {0x37};
        ASSERT_MEMORY_EQUAL(serverIsotpPhysRecvBuf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));
        done = true;
    }
    break;
}
    CLIENT_TEST_SEQUENCE_END(60000);
#undef MemorySize
    fclose(fd);
    remove(fname);
    TEST_TEARDOWN();
}

// ================================================
// System tests
// ================================================

void testSystemEcuReset() {
    CLIENT_SERVER_TEST_SETUP();
    serverCfg.userECUResetHandler = mockECUResetHandler;

    for (g_ms = 0; g_ms < client.settings.p2_ms + 2; g_ms++) {
        fixtureIsoTpPollLinks(&clientLink, &serverPhysLink, &serverFuncLink);
        Iso14229ClientPoll(&client);
        Iso14229ServerPoll(&server);
        if (0 == g_ms) {
            ECUReset(&client, kHardReset);
        }
    }
    ASSERT_INT_EQUAL(g_mockECUResetHandlerCallCount, 1);
    TEST_TEARDOWN();
}

/**
 * @brief run all tests
 */
int main() {
    testServerInit();
    testServer0x10DiagnosticSessionControlIsDisabledByDefault();
    testServer0x11DoesNotSendOrReceiveMessagesAfterECUReset();
    testServer0x22RDBI1();
    testServer0x27SecurityAccess();
    testServer0x27SecurityAccessAlreadyUnlocked();
    testServer0x31RCRRP();
    testServer0x34NotEnabled();
    testServer0x34DownloadData();
    testServer0x36TransferData();
    testServer0x3ESuppressPositiveResponse();
    testServer0x83DiagnosticSessionControl();

    testClientInit();
    testClientP2TimeoutExceeded();
    testClientP2TimeoutNotExceeded();
    testClientSuppressPositiveResponse();
    testClientBusy();
    testClientUnexpectedResponse();
    testClient0x11ECUReset();
    testClient0x11ECUResetNegativeResponse();
    testClient0x22RDBITxBufferTooSmall();
    testClient0x22RDBIUnpackResponse();
    testClient0x31RequestCorrectlyReceivedResponsePending();
    testClient0x34RequestDownload();
    testClient0x34UnpackRequestDownloadResponse();
    testClient0x36TransferData();
    testClient0x36TransferData2();

    testSystemEcuReset();
}
