#include "test_iso14229.h"
#include "iso14229.h"
#include "iso14229client.h"
#include "iso14229server.h"
#include "isotp-c/isotp.h"
#include "isotp-c/isotp_defines.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

uint32_t mockUserGetms();
int mockClientSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size);
int mockServerCANTransmit(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size);
enum Iso14229CANRxStatus mockServerCANRxPoll(uint32_t *arbitration_id, uint8_t *data,
                                             uint8_t *size);
void mockSrvFuncLinkDbg(const char *message, ...);
void mockSrvPhysLinkDbg(const char *message, ...);
void mockClientLinkDbg(const char *message, ...);

// ================================================
// Global Variables
// ================================================

#define CAN_MESSAGE_QUEUE_SIZE 10

// global state: memset() to zero in TEST_SETUP();
static struct {
    int ms; // simulated absolute time
    int t0; // marks a time point

    struct CANMessage serverRecvQueue[CAN_MESSAGE_QUEUE_SIZE];
    int serverRecvQueueIdx;
    struct CANMessage clientRecvQueue[CAN_MESSAGE_QUEUE_SIZE];
    int clientRecvQueueIdx;

    int g_serverSvcCallCount[ISO14229_NUM_SERVICES];
    Iso14229Service g_serverServices[ISO14229_NUM_SERVICES];

    IsoTpLink clientLink, srvPhysLink, srvFuncLink;
    uint8_t srvPhysLinkRxBuf[DEFAULT_ISOTP_BUFSIZE];
    uint8_t srvPhysLinkTxBuf[DEFAULT_ISOTP_BUFSIZE];
    uint8_t srvFuncLinkRxBuf[DEFAULT_ISOTP_BUFSIZE];
    uint8_t srvFuncLinkTxBuf[DEFAULT_ISOTP_BUFSIZE];
    uint8_t clientLinkRxBuf[DEFAULT_ISOTP_BUFSIZE];
    uint8_t clientLinkTxBuf[DEFAULT_ISOTP_BUFSIZE];

    uint8_t scratch[DEFAULT_ISOTP_BUFSIZE];
    uint16_t size;
    int ret;

    enum Iso14229ResponseCode userResponse;
} g;

static const struct IsoTpLinkConfig SRV_PHYS_LINK_DEFAULT_CONFIG = {
    .send_id = SERVER_SEND_ID,
    .send_buffer = g.srvPhysLinkTxBuf,
    .send_buf_size = sizeof(g.srvPhysLinkTxBuf),
    .recv_buffer = g.srvPhysLinkRxBuf,
    .recv_buf_size = sizeof(g.srvPhysLinkRxBuf),
    .user_get_ms = mockUserGetms,
    .user_send_can = mockServerCANTransmit,
    .user_debug = mockSrvPhysLinkDbg};

static const struct IsoTpLinkConfig CLIENT_LINK_DEFAULT_CONFIG = {
    .send_id = SERVER_PHYS_RECV_ID,
    .send_buffer = g.clientLinkTxBuf,
    .send_buf_size = sizeof(g.clientLinkTxBuf),
    .recv_buffer = g.clientLinkRxBuf,
    .recv_buf_size = sizeof(g.clientLinkRxBuf),
    .user_get_ms = mockUserGetms,
    .user_send_can = mockClientSendCAN,
    .user_debug = mockClientLinkDbg};

// ================================================
// common mock functions
// ---
// these are used in all tests
// ================================================

uint32_t mockUserGetms() { return g.ms; }

// ================================================
// isotp-c mock functions
// ================================================

void mockClientLinkDbg(const char *message, ...) {
    printf("CLIENT ISO-TP: ");
    va_list ap;
    va_start(ap, message);
    vprintf(message, ap);
    va_end(ap);
}

void mockSrvPhysLinkDbg(const char *message, ...) {
    printf("SRV PHYS ISO-TP: ");
    va_list ap;
    va_start(ap, message);
    vprintf(message, ap);
    va_end(ap);
}

void mockSrvFuncLinkDbg(const char *message, ...) {
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
// Iso14229Client Mock Functions
// ================================================

static enum Iso14229CANRxStatus mockClientCANRxPoll(uint32_t *arb_id, uint8_t *data, uint8_t *dlc) {
    if (g.clientRecvQueueIdx > 0) {
        struct CANMessage *msg = &g.clientRecvQueue[0];
        *arb_id = msg->arbId;
        memmove(data, msg->data, msg->size);
        *dlc = msg->size;
        ISO14229USERDEBUG("%06d c<0x%03x [%02d]: ", g.ms, msg->arbId, g.clientRecvQueueIdx);
        PRINTHEX(msg->data, msg->size);
        g.clientRecvQueueIdx--;
        memmove(g.clientRecvQueue, &g.clientRecvQueue[1], sizeof(struct CANMessage));
        return kCANRxSome;
    }
    return kCANRxNone;
}

// ================================================
// Fixtures
// ================================================

/**
 * @brief Sends CAN messages from client to an in-memory FIFO queue
 */
int mockClientSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    assert(size <= 8);
    assert(g.serverRecvQueueIdx < CAN_MESSAGE_QUEUE_SIZE);
    struct CANMessage *msg = &g.serverRecvQueue[g.serverRecvQueueIdx++];
    memmove(msg->data, data, size);
    msg->arbId = arbitration_id;
    msg->size = size;
    ISO14229USERDEBUG("%06d " ANSI_BRIGHT_GREEN "c>"
                      "0x%03x [%02d]: ",
                      g.ms, arbitration_id, g.serverRecvQueueIdx);
    PRINTHEX(data, size);
    ISO14229USERDEBUG(ANSI_RESET);
    return ISOTP_RET_OK;
}

/**
 * @brief Sends CAN messages from server to an in-memory FIFO queue
 */
int mockServerCANTransmit(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    assert(size <= 8);
    assert(g.clientRecvQueueIdx < CAN_MESSAGE_QUEUE_SIZE);
    struct CANMessage *msg = &g.clientRecvQueue[g.clientRecvQueueIdx++];
    memmove(msg->data, data, size);
    msg->arbId = arbitration_id;
    msg->size = size;
    ISO14229USERDEBUG("%06d " ANSI_BRIGHT_MAGENTA "s>0x%03x [%02d]: ", g.ms, arbitration_id,
                      g.clientRecvQueueIdx);
    PRINTHEX(data, size);
    ISO14229USERDEBUG(ANSI_RESET);
    return ISOTP_RET_OK;
}

enum Iso14229CANRxStatus mockServerCANRxPoll(uint32_t *arbitration_id, uint8_t *data,
                                             uint8_t *size) {
    if (g.serverRecvQueueIdx > 0) {
        struct CANMessage *msg = &g.serverRecvQueue[0];
        *arbitration_id = msg->arbId;
        memmove(data, msg->data, msg->size);
        *size = msg->size;
        // ISO14229USERDEBUG("%06d s<0x%03x [%02d]: ", g.ms, msg->arbId, g.serverRecvQueueIdx);
        // PRINTHEX(msg->data, msg->size);
        g.serverRecvQueueIdx--;
        memmove(g.serverRecvQueue, &g.serverRecvQueue[1], sizeof(struct CANMessage));
        return kCANRxSome;
    }
    return kCANRxNone;
}

void fixtureClientLinkProcess() {
    uint32_t arb_id;
    uint8_t data[8], dlc;

    switch (mockClientCANRxPoll(&arb_id, data, &dlc)) {
    case kCANRxSome:
        isotp_on_can_message(&g.clientLink, data, dlc);
        break;
    case kCANRxNone:
        break;
    default:
        assert(0);
    }
    isotp_poll(&g.clientLink);
}

void fixtureSrvLinksProcess() {
    uint32_t arb_id;
    uint8_t data[8], dlc;

    switch (mockServerCANRxPoll(&arb_id, data, &dlc)) {
    case kCANRxSome:
        switch (arb_id) {
        case SERVER_PHYS_RECV_ID:
            isotp_on_can_message(&g.srvPhysLink, data, dlc);
            break;
        case SERVER_FUNC_RECV_ID:
            isotp_on_can_message(&g.srvFuncLink, data, dlc);
            break;
        default:
            assert(0);
        }
        break;
    case kCANRxNone:
        break;
    default:
        assert(0);
    }
    isotp_poll(&g.srvPhysLink);
    isotp_poll(&g.srvFuncLink);
}

// ================================================
// Server tests
// ================================================

void testServerInit() {
    TEST_SETUP();
    Iso14229Server srv;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    Iso14229ServerInit(&srv, &cfg);
    TEST_TEARDOWN();
}

void testServer0x10DiagnosticSessionControlIsDisabledByDefault() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t MOCK_DATA[] = {0x10, 0x02};
    const uint8_t CORRECT_RESPONSE[] = {0x7f, 0x10, 0x11};

    // sending a diagnostic session control request
    isotp_send(&g.clientLink, MOCK_DATA, sizeof(MOCK_DATA));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < 2); // (in a reasonable time)
    }

    // that is negative, indicating that diagnostic session control is disabled by default
    ASSERT_MEMORY_EQUAL(CORRECT_RESPONSE, g.scratch, sizeof(CORRECT_RESPONSE));
    TEST_TEARDOWN();
}

// Special-case of ECU reset service
// ISO-14229-1 2013 9.3.1:
// on the behaviour of the ECU from the time following the positive response message to the ECU
// reset request: It is recommended that during this time the ECU does not accept any request
// messages and send any response messages.
void testServer0x11DoesNotSendOrReceiveMessagesAfterECUReset() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    cfg.userECUResetHandler = mockECUResetHandler;
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t MOCK_DATA[] = {0x11, 0x01};
    const uint8_t EXPECTED_RESPONSE[] = {0x51, 0x01};

    // Sending an ECU reset
    isotp_send(&g.clientLink, MOCK_DATA, sizeof(MOCK_DATA));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < 2); // (in a reasonable time)
    }

    // that matches the expected response.
    ASSERT_INT_EQUAL(g.size, sizeof(EXPECTED_RESPONSE));
    ASSERT_MEMORY_EQUAL(EXPECTED_RESPONSE, g.scratch, sizeof(EXPECTED_RESPONSE));

    // The ECU reset handler should have been called.
    ASSERT_INT_EQUAL(g_mockECUResetHandlerCallCount, 1);

    // Sending a second ECU reset
    isotp_send(&g.clientLink, MOCK_DATA, sizeof(MOCK_DATA));

    // should not receive a response until the server is reset
    while (g.ms++ < 100) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        g.ret = isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size);
        ASSERT_INT_EQUAL(g.ret, ISOTP_RET_NO_DATA);
    }
    TEST_TEARDOWN();
}

void testServer0x22RDBI1() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    cfg.userRDBIHandler = mockUserRdbiHandler;
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t MOCK_DATA[] = {0x22, 0xF1, 0x90};
    const uint8_t CORRECT_RESPONSE[] = {0x62, 0xF1, 0x90, 0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30,
                                        0x34, 0x33, 0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
    // sending an RDBI request
    isotp_send(&g.clientLink, MOCK_DATA, sizeof(MOCK_DATA));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < 10); // (in a reasonable time)
    }

    // that matches the correct response
    ASSERT_INT_EQUAL(sizeof(CORRECT_RESPONSE), g.size);
    ASSERT_MEMORY_EQUAL(CORRECT_RESPONSE, g.scratch, sizeof(CORRECT_RESPONSE));
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
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    cfg.userSecurityAccessGenerateSeed = mockSecurityAccessGenerateSeed;
    cfg.userSecurityAccessValidateKey = mockSecurityAccessValidateKey;
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    const uint8_t SEED_RESPONSE[] = {0x67, 0x01, 0x36, 0x57};
    const uint8_t UNLOCK_REQUEST[] = {0x27, 0x02, 0xC9, 0xA9};
    const uint8_t UNLOCK_RESPONSE[] = {0x67, 0x02};

    // the server security level after initialization should be 0
    ASSERT_INT_EQUAL(server.status.securityLevel, 0);

    // sending a seed request
    isotp_send(&g.clientLink, SEED_REQUEST, sizeof(SEED_REQUEST));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < SERVER_DEFAULT_P2_MS); // (in a reasonable time)
    }

    // that matches the correct response
    ASSERT_INT_EQUAL(sizeof(SEED_RESPONSE), g.size);
    ASSERT_MEMORY_EQUAL(SEED_RESPONSE, g.scratch, sizeof(SEED_RESPONSE));

    // subsequently sending an unlock request
    isotp_send(&g.clientLink, UNLOCK_REQUEST, sizeof(UNLOCK_REQUEST));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < 2 * SERVER_DEFAULT_P2_MS); // (in a reasonable time)
    }

    // that matches the correct response
    ASSERT_INT_EQUAL(sizeof(UNLOCK_RESPONSE), g.size);
    ASSERT_MEMORY_EQUAL(UNLOCK_RESPONSE, g.scratch, sizeof(UNLOCK_RESPONSE));

    // Additionally, the security level should now be 1
    ASSERT_INT_EQUAL(server.status.securityLevel, 1);
    TEST_TEARDOWN();
}

// ISO14229-1 2013 9.4.5.3
void testServer0x27SecurityAccessAlreadyUnlocked() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    cfg.userSecurityAccessGenerateSeed = mockSecurityAccessGenerateSeed;
    cfg.userSecurityAccessValidateKey = mockSecurityAccessValidateKey;
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    const uint8_t ALREADY_UNLOCKED_RESPONSE[] = {0x67, 0x01, 0x00, 0x00};

    // when the security level is already set to 1
    server.status.securityLevel = 1;

    // sending a seed request
    isotp_send(&g.clientLink, SEED_REQUEST, sizeof(SEED_REQUEST));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < SERVER_DEFAULT_P2_MS); // (in a reasonable time)
    }

    // that matches the correct response
    ASSERT_INT_EQUAL(sizeof(ALREADY_UNLOCKED_RESPONSE), g.size);
    ASSERT_MEMORY_EQUAL(ALREADY_UNLOCKED_RESPONSE, g.scratch, sizeof(ALREADY_UNLOCKED_RESPONSE));

    TEST_TEARDOWN();
}

static enum Iso14229ResponseCode testServer0x31RCRRPMockRoutineControl(
    const struct Iso14229ServerStatus *status, enum RoutineControlType routineControlType,
    uint16_t routineIdentifier, Iso14229RoutineControlArgs *args) {
    (void)status;
    (void)routineControlType;
    (void)routineIdentifier;
    (void)args;
    return g.userResponse;
}

// ISO-14229-1 2013 Table A.1 Byte Value 0x78: requestCorrectlyReceived-ResponsePending
// "This NRC is in general supported by each diagnostic service".
void testServer0x31RCRRP() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    cfg.userRoutineControlHandler = testServer0x31RCRRPMockRoutineControl;
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t REQUEST[] = {0x31, 0x01, 0x12, 0x34};
    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // Request Correctly Received - Response Pending
    const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};

    // When a user handler initially returns RRCRP
    g.userResponse = kRequestCorrectlyReceived_ResponsePending;

    // sending a request to the server
    isotp_send(&g.clientLink, REQUEST, sizeof(REQUEST));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < cfg.p2_ms); // in p2 ms
    }

    // a RequestCorrectlyReceived-ResponsePending response.
    ASSERT_INT_EQUAL(sizeof(RCRRP), g.size);
    ASSERT_MEMORY_EQUAL(RCRRP, g.scratch, sizeof(RCRRP));

    // The server should again respond
    g.t0 = g.ms;
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ - g.t0 < SERVER_DEFAULT_P2_STAR_MS); // in p2_star ms
    }

    // with another RequestCorrectlyReceived-ResponsePending response.
    ASSERT_INT_EQUAL(sizeof(RCRRP), g.size);
    ASSERT_MEMORY_EQUAL(RCRRP, g.scratch, sizeof(RCRRP));

    // When the user handler now returns a positive response
    g.userResponse = kPositiveResponse;
    g.t0 = g.ms;

    // the server should respond
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ - g.t0 < SERVER_DEFAULT_P2_MS); // in p2_ms
    }

    // with a positive response
    ASSERT_INT_EQUAL(sizeof(POSITIVE_RESPONSE), g.size);
    ASSERT_MEMORY_EQUAL(POSITIVE_RESPONSE, g.scratch, sizeof(POSITIVE_RESPONSE));
    TEST_TEARDOWN();
}

void testServer0x34NotEnabled() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t REQUEST_DOWNLOAD_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20,
                                                0x00, 0x00, 0xFF, 0xFF};
    const uint8_t NEGATIVE_RESPONSE[] = {0x7F, 0x34, 0x11};

    // when no requestDownloadHandler is installed,
    // sending a request to the server
    isotp_send(&g.clientLink, REQUEST_DOWNLOAD_REQUEST, sizeof(REQUEST_DOWNLOAD_REQUEST));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < cfg.p2_ms); // in p2 ms
    }

    // a kServiceNotSupported response
    ASSERT_INT_EQUAL(sizeof(NEGATIVE_RESPONSE), g.size);
    ASSERT_MEMORY_EQUAL(NEGATIVE_RESPONSE, g.scratch, sizeof(NEGATIVE_RESPONSE));
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

static enum Iso14229ResponseCode testServer0x34DownloadDataMockuserRequestDownloadHandler(
    const struct Iso14229ServerStatus *status, void *memoryAddress, size_t memorySize,
    uint8_t dataFormatIdentifier, Iso14229DownloadHandler **handler,
    uint16_t *maxNumberOfBlockLength) {
    (void)status;
    static Iso14229DownloadHandler mockHandler = {
        .onExit = testServer0x34DownloadDataMockHandlerOnExit,
        .onTransfer = testServer0x34DownloadDataMockHandlerOnTransfer,
        .userCtx = NULL,
    };
    ASSERT_INT_EQUAL(0x11, dataFormatIdentifier);
    ASSERT_PTR_EQUAL((void *)0x602000, memoryAddress);
    ASSERT_INT_EQUAL(0x00FFFF, memorySize);
    *handler = &mockHandler;
    *maxNumberOfBlockLength = 0x0081;
    return kPositiveResponse;
}

void testServer0x34DownloadData() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    // when a handler is installed that implements ISO14229-1:2013 Table 415
    cfg.userRequestDownloadHandler = testServer0x34DownloadDataMockuserRequestDownloadHandler;
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    const uint8_t REQUEST_DOWNLOAD_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20,
                                                0x00, 0x00, 0xFF, 0xFF};
    const uint8_t POSITIVE_RESPONSE[] = {0x74, 0x20, 0x00, 0x81};

    // sending this request to the server
    isotp_send(&g.clientLink, REQUEST_DOWNLOAD_REQUEST, sizeof(REQUEST_DOWNLOAD_REQUEST));

    // should result in a response
    while (ISOTP_RET_OK != isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size)) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        assert(g.ms++ < cfg.p2_ms); // in less than p2 ms
    }

    // a positive response matching ISO14229-1:2013 Table 415
    ASSERT_INT_EQUAL(sizeof(POSITIVE_RESPONSE), g.size);
    ASSERT_MEMORY_EQUAL(POSITIVE_RESPONSE, g.scratch, sizeof(POSITIVE_RESPONSE));
    TEST_TEARDOWN();
}

// #define TEST_0x36_MOCK_DATA 0xF0, 0x00, 0xBA, 0xBA
// static enum Iso14229ResponseCode
// testServer0x36TransferDataMockHandlerOnTransfer(const struct Iso14229ServerStatus *status,
//                                                 void *userCtx, const uint8_t *data, uint32_t len)
//                                                 {
//     (void)status;
//     (void)userCtx;
//     const uint8_t MOCK_DATA[] = {TEST_0x36_MOCK_DATA};
//     ASSERT_INT_EQUAL(sizeof(MOCK_DATA), len);
//     ASSERT_MEMORY_EQUAL(MOCK_DATA, data, len);
//     return kPositiveResponse;
// }

// Iso14229DownloadHandler testServer0x36TransferDataMockHandler = {
//     .onTransfer = testServer0x36TransferDataMockHandlerOnTransfer,
// };

// void testServer0x36TransferData() {
//     Iso14229Server server;
//     Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
//     // when a handler is installed that implements ISO14229-1:2013 Table 415
//     cfg.userRequestDownloadHandler = testServer0x34DownloadDataMockuserRequestDownloadHandler;
//     Iso14229ServerInit(&server, &cfg);
//     IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

//     SERVER_TEST_SETUP();
//     server.downloadHandler = &testServer0x36TransferDataMockHandler;
//     Iso14229DownloadHandlerInit(server.downloadHandler, 0xFFFF);
//     const uint8_t TRANSFER_DATA_REQUEST[] = {0x36, 0x01, TEST_0x36_MOCK_DATA};
// #undef TEST_0x36_MOCK_DATA
//     const uint8_t TRANSFER_DATA_RESPONSE[] = {0x76, 0x01};
//     SERVER_TEST_SEQUENCE_BEGIN();
// case 0:
//     SERVER_TEST_CLIENT_SEND(TRANSFER_DATA_REQUEST);
//     break;
// case 1:
//     SERVER_TEST_AWAIT_RESPONSE(TRANSFER_DATA_RESPONSE);
//     break;
// case 2:
//     done = true;
//     break;
//     SERVER_TEST_SEQUENCE_END(server.cfg->p2_ms * 3);
//     ASSERT_INT_EQUAL(server.downloadHandler->numBytesTransferred, 4);
//     ASSERT_INT_EQUAL(server.downloadHandler->blockSequenceCounter, 2);
//     TEST_TEARDOWN();
// }

/* ISO14229-1 2013 Table 72 */
void testServer0x3ESuppressPositiveResponse() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    // when the suppressPositiveResponse bit is set
    const uint8_t REQUEST[] = {0x3E, 0x80};

    // sending this request
    isotp_send(&g.clientLink, REQUEST, sizeof(REQUEST));

    // should result in no response.
    while (g.ms++ < cfg.p2_ms) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        g.ret = isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size);
        ASSERT_INT_EQUAL(ISOTP_RET_NO_DATA, g.ret);
        ASSERT_INT_EQUAL(g.size, 0);
    }
    TEST_TEARDOWN();
}

void testServer0x83DiagnosticSessionControl() {
    TEST_SETUP();
    Iso14229Server server;
    Iso14229ServerConfig cfg = DEFAULT_SERVER_CONFIG();
    cfg.userDiagnosticSessionControlHandler = mockDiagnosticSessionControlHandler;
    Iso14229ServerInit(&server, &cfg);
    IsoTpInitLink(&g.clientLink, &CLIENT_LINK_DEFAULT_CONFIG);

    // the server sessionType after initialization should be kDefaultSession.
    ASSERT_INT_EQUAL(server.status.sessionType, kDefaultSession);

    // When the suppressPositiveResponse bit is set
    const uint8_t REQUEST[] = {0x10, 0x83};

    // sending this request
    isotp_send(&g.clientLink, REQUEST, sizeof(REQUEST));

    // should result in no response.
    while (g.ms++ < cfg.p2_ms) {
        Iso14229ServerPoll(&server);
        fixtureClientLinkProcess();
        g.ret = isotp_receive(&g.clientLink, g.scratch, sizeof(g.scratch), &g.size);
        ASSERT_INT_EQUAL(ISOTP_RET_NO_DATA, g.ret);
        ASSERT_INT_EQUAL(g.size, 0);
    }

    // and the server sessionType should have changed
    ASSERT_INT_EQUAL(server.status.sessionType, kExtendedDiagnostic);

    TEST_TEARDOWN();
}

// ================================================
// Client tests
// ================================================

void testClientInit() {
    TEST_SETUP();
    Iso14229Client client;
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);
    TEST_TEARDOWN();
}

void testClientP2TimeoutExceeded() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // Sending an ECU reset
    ECUReset(&client, kHardReset);

    // and not receiving a response after approximately p2 ms
    while (g.ms++ < cfg.p2_ms + 3) {
        Iso14229ClientPoll(&client);
    }

    // should result in kRequestTimedOut
    ASSERT_INT_EQUAL(kRequestTimedOut, client.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.state);
    TEST_TEARDOWN();
}

void testClientP2TimeoutNotExceeded() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // a client that sends an ECU reset
    ECUReset(&client, kHardReset);

    // which receives a positive response
    const uint8_t POSITIVE_RESPONSE[] = {0x51, 0x01};
    isotp_send(&g.srvPhysLink, POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE));

    // should return to the idle state
    while (kRequestStateIdle != client.state) {
        Iso14229ClientPoll(&client);
        assert(g.ms++ < cfg.p2_ms); // before p2 ms has elapsed
    }

    // and should have no error.
    ASSERT_INT_EQUAL(kRequestNoError, client.err);
    TEST_TEARDOWN();
}

void testClientSuppressPositiveResponse() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // Setting the suppressPositiveResponse flag before sending a request
    client.suppressPositiveResponse = true;
    ECUReset(&client, kHardReset);

    // and not receiving a response after approximately p2 ms
    while (g.ms++ < cfg.p2_ms + 3) {
        Iso14229ClientPoll(&client);
    }

    // should not result in an error.
    ASSERT_INT_EQUAL(kRequestNoError, client.err);
    ASSERT_INT_EQUAL(kRequestStateIdle, client.state);
    TEST_TEARDOWN();
}

void testClientBusy() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // Sending a request should not return an error
    ASSERT_INT_EQUAL(kRequestNoError, ECUReset(&client, kHardReset));

    // unless there is an existing unresolved request
    ASSERT_INT_EQUAL(kRequestNotSentBusy, ECUReset(&client, kHardReset));

    TEST_TEARDOWN();
}

void testClientUnexpectedResponse() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // sending an ECU reset
    ECUReset(&client, kHardReset);

    // The correct response SID to EcuReset (0x11) is 0x51.
    const uint8_t WEIRD_RESPONSE[] = {0x50, 0x01}; // incorrect, unexpected

    isotp_send(&g.srvPhysLink, WEIRD_RESPONSE, sizeof(WEIRD_RESPONSE));

    // should return to the idle state
    while (kRequestStateIdle != client.state) {
        Iso14229ClientPoll(&client);
        assert(g.ms++ < cfg.p2_ms); // before p2 ms has elapsed
    }

    // with a kRequestErrorResponseSIDMismatch error.
    ASSERT_INT_EQUAL(kRequestErrorResponseSIDMismatch, client.err);
    TEST_TEARDOWN();
}

void testClient0x11ECUReset() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // sending an ECUReset of type kHardReset
    ECUReset(&client, kHardReset);

    // should send these bytes to the ISO-TP layer
    const uint8_t HARD_RESET_REQUEST[] = {0x11, 0x01};
    ASSERT_MEMORY_EQUAL(g.clientLinkTxBuf, HARD_RESET_REQUEST, sizeof(HARD_RESET_REQUEST));
    ASSERT_INT_EQUAL(sizeof(HARD_RESET_REQUEST), g.clientLink.send_size);

    TEST_TEARDOWN();
}

void testClient0x11ECUResetNegativeResponse() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // A client that sends an ECU reset
    ECUReset(&client, kHardReset);

    // and receives a negative response
    const uint8_t NEG_RESPONSE[] = {0x7F, 0x11, 0x10};
    isotp_send(&g.srvPhysLink, NEG_RESPONSE, sizeof(NEG_RESPONSE));

    // should return to the idle state
    while (kRequestStateIdle != client.state) {
        Iso14229ClientPoll(&client);
        assert(g.ms++ < cfg.p2_ms); // before p2 ms has elapsed
    }

    // with a kRequestErrorNegativeResponse error
    ASSERT_INT_EQUAL(kRequestErrorNegativeResponse, client.err);
    TEST_TEARDOWN();
}

void testClient0x11ECUResetNegativeResponseNoError() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // A client that sets the negativeResponseIsError flag to false
    client.negativeResponseIsError = false;
    // before sending an ECU reset
    ECUReset(&client, kHardReset);

    // and receiving a negative response
    const uint8_t NEG_RESPONSE[] = {0x7F, 0x11, 0x10};
    isotp_send(&g.srvPhysLink, NEG_RESPONSE, sizeof(NEG_RESPONSE));

    // should return to the idle state
    while (kRequestStateIdle != client.state) {
        Iso14229ClientPoll(&client);
        assert(g.ms++ < cfg.p2_ms); // before p2 ms has elapsed
    }

    // with no error
    ASSERT_INT_EQUAL(kRequestNoError, client.err);
    TEST_TEARDOWN();
}

void testClient0x22RDBITxBufferTooSmall() {
    TEST_SETUP();
    Iso14229Client client;
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // attempting to send a request payload of 6 bytes
    uint16_t didList[] = {0x0001, 0x0002, 0x0003};

    // which is larger than the underlying buffer
    client.link->send_buf_size = 4;

    // should return an error
    ASSERT_INT_EQUAL(kRequestNotSentInvalidArgs,
                     ReadDataByIdentifier(&client, didList, ARRAY_SZ(didList)))

    // and no data should be sent
    ASSERT_INT_EQUAL(client.link->send_size, 0);

    TEST_TEARDOWN();
}

void testClient0x22RDBIUnpackResponse() {
    TEST_SETUP();
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
    TEST_SETUP();
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    Iso14229Client client;
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    cfg.p2_ms = 10;
    cfg.p2_star_ms = 50;
    iso14229ClientInit(&client, &cfg);

    // When a request is sent
    RoutineControl(&client, kStartRoutine, 0x1234, NULL, 0);

    // that receives an RCRRP response
    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78}; // RequestCorrectly-ReceievedResponsePending
    isotp_send(&g.srvPhysLink, RCRRP, sizeof(RCRRP));

    // that remains unresolved at a time between p2 ms and p2 star ms
    while (g.ms++ < 30) {
        Iso14229ClientPoll(&client);
    }

    // the client should still be pending.
    ASSERT_INT_EQUAL(kRequestStateSentAwaitResponse, client.state)

    // When the server sends a positive response
    const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
    isotp_send(&g.srvPhysLink, POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE));
    g.t0 = g.ms;

    // the client should return to the idle state
    while (kRequestStateIdle != client.state) {
        Iso14229ClientPoll(&client);
        assert(g.ms++ - g.t0 < cfg.p2_ms); // before p2 ms has elapsed
    }

    // with no error
    ASSERT_INT_EQUAL(client.err, kRequestNoError);
    TEST_TEARDOWN();
}

void testClient0x34RequestDownload() {
    TEST_SETUP();
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    Iso14229Client client;
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // When RequestDownload is called with these arguments
    ASSERT_INT_EQUAL(kRequestNoError, RequestDownload(&client, 0x11, 0x33, 0x602000, 0x00FFFF));

    // the bytes sent should match ISO14229-1 2013 Table 415
    const uint8_t CORRECT_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    ASSERT_MEMORY_EQUAL(g.clientLinkTxBuf, CORRECT_REQUEST, sizeof(CORRECT_REQUEST));
    ASSERT_INT_EQUAL(sizeof(CORRECT_REQUEST), g.clientLink.send_size);
    TEST_TEARDOWN();
}

void testClient0x34UnpackRequestDownloadResponse() {
    TEST_SETUP();
    struct RequestDownloadResponse unpacked;

    // When the following raw bytes are received
    uint8_t RESPONSE[] = {0x74, 0x20, 0x00, 0x81};
    struct Iso14229Response resp = {
        .buf = RESPONSE,
        .buffer_size = sizeof(RESPONSE),
        .len = sizeof(RESPONSE),
    };

    enum Iso14229ClientRequestError err = UnpackRequestDownloadResponse(&resp, &unpacked);

    // they should unpack without error
    ASSERT_INT_EQUAL(err, kRequestNoError);
    ASSERT_INT_EQUAL(unpacked.maxNumberOfBlockLength, 0x81);
}

void testClient0x36TransferData() {
    TEST_SETUP();
    IsoTpInitLink(&g.srvPhysLink, &SRV_PHYS_LINK_DEFAULT_CONFIG);
    Iso14229Client client;
    struct Iso14229ClientConfig cfg = DEFAULT_CLIENT_CONFIG();
    iso14229ClientInit(&client, &cfg);

    // This test is large because it implements ISO14229-1 2013 14.5.5
    // It would be perhaps be better implemented with the
    // iso14229ClientSequenceRunBlocking API instead.

#define MemorySize (0x00FFFF)

    // Create some source data and write it to a file
    const char *fname = "testClient0x36TransferData2.dat";
    uint8_t SRC_DATA[MemorySize] = {0};
    for (unsigned int i = 0; i < sizeof(SRC_DATA); i++) {
        SRC_DATA[i] = i & 0xFF;
    }
    FILE *fd = fopen(fname, "wb+");
    assert(sizeof(SRC_DATA) == fwrite(SRC_DATA, 1, sizeof(SRC_DATA), fd));
    rewind(fd);

    // see example
    const uint16_t maximumNumberOfBlockLength = 0x0081;
    uint32_t blockNr = 0;

    // when RequestDownload is called
    RequestDownload(&client, 0x11, 0x33, 0x602000, MemorySize);

    // the server link should receive it
    while (ISOTP_RET_OK != isotp_receive(&g.srvPhysLink, g.scratch, sizeof(g.scratch), &g.size)) {
        fixtureSrvLinksProcess();
        Iso14229ClientPoll(&client);
        assert(g.ms++ < cfg.p2_ms); // in p2 ms
    }

    // The bytes received should match Table 415
    ASSERT_INT_EQUAL(g.size, 9);
    const uint8_t CORRECT_0x34_REQUEST[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    ASSERT_MEMORY_EQUAL(g.srvPhysLinkRxBuf, CORRECT_0x34_REQUEST, sizeof(CORRECT_0x34_REQUEST));

    // when the server sends this positive response
    const uint8_t RESPONSE[] = {0x74, 0x20, 0x00, 0x81};
    isotp_send(&g.srvPhysLink, RESPONSE, sizeof(RESPONSE));

    // the client should return to the idle state in under p2 ms
    g.t0 = g.ms;
    while (kRequestStateIdle != client.state) {
        fixtureSrvLinksProcess();
        Iso14229ClientPoll(&client);
        assert(g.ms++ - g.t0 < client.p2_ms);
    }
    // and have no errors.
    ASSERT_INT_EQUAL(kRequestNoError, client.err);

    assert(!ferror(fd));

    // Until the file has been exhausted. . .
    while (!feof(fd)) {

        // the client should send chunks of data from the file.
        TransferDataStream(&client, ++blockNr & 0xFF, maximumNumberOfBlockLength, fd);

        // When the server responds positively,
        const uint8_t RESPONSE[] = {0x76, blockNr};
        isotp_send(&g.srvPhysLink, RESPONSE, sizeof(RESPONSE));

        g.t0 = g.ms;
        // The client should return to the idle state
        while (kRequestStateIdle != client.state) {
            fixtureSrvLinksProcess();
            Iso14229ClientPoll(&client);
            assert(g.ms++ - g.t0 < client.p2_ms); // in under p2 ms
        }
        ASSERT_INT_EQUAL(kRequestNoError, client.err); // with no error.

        // and the server link should have received data
        g.ret = isotp_receive(&g.srvPhysLink, g.scratch, sizeof(g.scratch), &g.size);
        ASSERT_INT_EQUAL(ISOTP_RET_OK, g.ret);

        assert(g.ms < 60000); // timeout: TransferDataStream isn't reading the fd.
    }

    // The transfer is complete.
    // The last message contains the final three bytes of the source data.
    // Create a reference:
    uint8_t REQUEST[5] = {0x36, 0x05, 0x0, 0x0, 0x0};
    for (int i = 0; i < 3; i++) {
        REQUEST[2 + i] = SRC_DATA[(MemorySize - 1) - 2 + i];
    }
    // The data received by the server at the should match the reference.
    ASSERT_INT_EQUAL(g.srvPhysLink.receive_size, sizeof(REQUEST));
    ASSERT_MEMORY_EQUAL(g.srvPhysLinkRxBuf, REQUEST, sizeof(REQUEST)); // Table 419
    ASSERT_INT_EQUAL(blockNr, 517);                                    // 14.5.5.1.1

    // When RequestTransferExit is called
    RequestTransferExit(&client);

    // the server link should receive data
    g.t0 = g.ms;
    while (ISOTP_RET_OK != isotp_receive(&g.srvPhysLink, g.scratch, sizeof(g.scratch), &g.size)) {
        fixtureSrvLinksProcess();
        Iso14229ClientPoll(&client);
        assert(g.ms++ - g.t0 < cfg.p2_ms); // in p2 ms
    }

    // and the data should look like this
    const uint8_t CORRECT_0x37_REQUEST[] = {0x37};
    ASSERT_INT_EQUAL(g.size, sizeof(CORRECT_0x37_REQUEST));
    ASSERT_MEMORY_EQUAL(g.srvPhysLinkRxBuf, CORRECT_0x37_REQUEST, sizeof(CORRECT_0x37_REQUEST));

#undef MemorySize
    fclose(fd);
    remove(fname);
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
    // testServer0x36TransferData();
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
    testClient0x11ECUResetNegativeResponseNoError();
    testClient0x22RDBITxBufferTooSmall();
    testClient0x22RDBIUnpackResponse();
    testClient0x31RequestCorrectlyReceivedResponsePending();
    testClient0x34RequestDownload();
    testClient0x34UnpackRequestDownloadResponse();
    testClient0x36TransferData();
}
