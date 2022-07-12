#define ISO14229USERDEBUG printf
#include "../iso14229server.h"
#include "server.h"
#include "shared.h"
#include "port.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void sessionTimeout();
static enum Iso14229ResponseCode ECUResetHandler(const struct Iso14229ServerStatus *status,
                                                 uint8_t resetType, uint8_t *powerDownTime);
static enum Iso14229ResponseCode
diagnosticSessionControlHandler(const struct Iso14229ServerStatus *status,
                                enum Iso14229DiagnosticSessionType type);
static enum Iso14229ResponseCode
securityAccessGenerateSeed(const struct Iso14229ServerStatus *status, uint8_t level,
                           const uint8_t *in_data, uint16_t in_size, uint8_t *out_data,
                           uint16_t out_bufsize, uint16_t *out_size);
static enum Iso14229ResponseCode
securityAccessValidateKey(const struct Iso14229ServerStatus *status, uint8_t level,
                          const uint8_t *key, uint16_t size);
static enum Iso14229ResponseCode rdbiHandler(const struct Iso14229ServerStatus *status,
                                             uint16_t dataId, const uint8_t **data_location,
                                             uint16_t *len);
static enum Iso14229ResponseCode wdbiHandler(const struct Iso14229ServerStatus *status,
                                             uint16_t dataId, const uint8_t *data, uint16_t len);

#define ISOTP_BUFSIZE 256

static uint8_t isotpPhysRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpPhysSendBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncSendBuf[ISOTP_BUFSIZE];
static IsoTpLink isotpPhysLink;
static IsoTpLink isotpFuncLink;

static const Iso14229ServerConfig cfg = {
    .phys_recv_id = SRV_PHYS_RECV_ID,
    .func_recv_id = SRV_FUNC_RECV_ID,
    .send_id = SRV_SEND_ID,
    .phys_link = &isotpPhysLink,
    .func_link = &isotpFuncLink,
    .phys_link_receive_buffer = isotpPhysRecvBuf,
    .phys_link_recv_buf_size = sizeof(isotpPhysRecvBuf),
    .phys_link_send_buffer = isotpPhysSendBuf,
    .phys_link_send_buf_size = sizeof(isotpPhysSendBuf),
    .func_link_receive_buffer = isotpFuncRecvBuf,
    .func_link_recv_buf_size = sizeof(isotpFuncRecvBuf),
    .func_link_send_buffer = isotpFuncSendBuf,
    .func_link_send_buf_size = sizeof(isotpFuncSendBuf),
    .userGetms = portGetms,
    .userCANTransmit = portSendCAN,
    .userCANRxPoll = portCANRxPoll,
    .userSessionTimeoutCallback = sessionTimeout,
    .userECUResetHandler = ECUResetHandler,
    .userDiagnosticSessionControlHandler = diagnosticSessionControlHandler,
    .userSecurityAccessGenerateSeed = securityAccessGenerateSeed,
    .userSecurityAccessValidateKey = securityAccessValidateKey,
    .userRDBIHandler = rdbiHandler,
    .userWDBIHandler = wdbiHandler,
    .p2_ms = 50,
    .p2_star_ms = 2000,
    .s3_ms = 5000,
};

static Iso14229Server srv;
static uint8_t ecu_reset_scheduled = 0;
static int ecu_reset_timer = 0;
static struct RWDBIData {
    uint8_t d0001;
    int8_t d0002;
    uint16_t d0003;
    int16_t d0004;
} myData = {0};

// 用初始化服务器实例来简单模拟一个ECU复位
// mock an ECU reset by resetting the server
static void mockECUReset() {
    printf("Resetting ECU (type: %d)\n", ecu_reset_scheduled);
    switch (ecu_reset_scheduled) {
    case kHardReset:
    case kSoftReset:
        Iso14229ServerInit(&srv, &cfg);
        break;
    case RESET_TYPE_EXIT:
        printf("server exiting. . .\n");
        exit(0);
    default:
        printf("unknown reset type %d\n", ecu_reset_scheduled);
        break;
    }
    ecu_reset_scheduled = 0;
}

static void sessionTimeout() {
    printf("server session timed out!\n");
    mockECUReset();
}

static void scheduleReset(int when, uint8_t resetType) {
    ecu_reset_scheduled = resetType;
    ecu_reset_timer = portGetms() + when;
    printf("scheduled ECUReset for %d ms from now\n", when);
}

static enum Iso14229ResponseCode ECUResetHandler(const struct Iso14229ServerStatus *status,
                                                 uint8_t resetType, uint8_t *powerDownTime) {
    (void)status;
    (void)powerDownTime;
    printf("got ECUReset request of type %x\n", resetType);
    switch (resetType) {
    case kHardReset:
    case kSoftReset:
    case RESET_TYPE_EXIT:
        scheduleReset(50, resetType);
        return kPositiveResponse;
        break;
    default:
        return kSubFunctionNotSupported;
    }
}

static enum Iso14229ResponseCode
diagnosticSessionControlHandler(const struct Iso14229ServerStatus *status,
                                enum Iso14229DiagnosticSessionType type) {
    switch (type) {
    case kDefaultSession:
        return kPositiveResponse;
    case kProgrammingSession:
    case kExtendedDiagnostic:
        if (status->securityLevel > 0) {
            return kPositiveResponse;
        } else {
            return kSecurityAccessDenied;
        }
        break;
    default:
        return kSubFunctionNotSupported;
    }
}

static enum Iso14229ResponseCode
securityAccessGenerateSeed(const struct Iso14229ServerStatus *status, uint8_t level,
                           const uint8_t *in_data, uint16_t in_size, uint8_t *out_data,
                           uint16_t out_bufsize, uint16_t *out_size) {
    (void)status;
    (void)level;
    (void)in_data;
    (void)in_size;
    const uint8_t seed[] = {1, 2, 3, 4};
    if (out_bufsize < sizeof(seed)) {
        return kResponseTooLong;
    }
    *out_size = sizeof(seed);
    if (level == status->securityLevel) {
        memset(out_data, 0, sizeof(seed));
    } else {
        memmove(out_data, seed, sizeof(seed));
    }
    return kPositiveResponse;
}

static enum Iso14229ResponseCode
securityAccessValidateKey(const struct Iso14229ServerStatus *status, uint8_t level,
                          const uint8_t *key, uint16_t size) {
    (void)status;
    (void)level;
    (void)key;
    (void)size;
    // doValidation(key, size);
    return kPositiveResponse;
}

static enum Iso14229ResponseCode rdbiHandler(const struct Iso14229ServerStatus *status,
                                             uint16_t dataId, const uint8_t **data_location,
                                             uint16_t *len) {
    (void)status;
    static const uint8_t msg[] = "I'm a UDS server    ";
    switch (dataId) {
    case 0x0001:
        *data_location = &myData.d0001;
        *len = DID_0x0001_LEN;
        break;
    case 0x0008:
        *data_location = msg;
        *len = DID_0x0008_LEN;
        break;
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}
static enum Iso14229ResponseCode wdbiHandler(const struct Iso14229ServerStatus *status,
                                             uint16_t dataId, const uint8_t *data, uint16_t len) {
    (void)status;
    switch (dataId) {
    case 0x0001:
        if (len != sizeof(myData.d0001)) {
            return kIncorrectMessageLengthOrInvalidFormat;
        }
        myData.d0001 = *data;
        break;
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}

// =====================================
// STEP 2: initialize the server
// =====================================

int run_server_blocking() {

    Iso14229ServerInit(&srv, &cfg);

    // =====================================
    // STEP 3: poll the server
    // =====================================

    printf("server up, polling . . .\n");
    while (!port_should_exit) {
        Iso14229ServerPoll(&srv);
        if (ecu_reset_scheduled && Iso14229TimeAfter(ecu_reset_timer, portGetms())) {
            mockECUReset();
        }
        portYieldms(10);
    }
    printf("server exiting\n");
    return 0;
}
