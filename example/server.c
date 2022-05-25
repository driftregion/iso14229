
#include "../iso14229server.h"
#include "host.h"
#include <assert.h>
#include <stdio.h>

#define SRV_PHYS_RECV_ID 0x7A0 // server listens for physical (1:1) messages on this CAN ID
#define SRV_FUNC_RECV_ID 0x7A1 // server listens for functional (1:n) messages on this CAN ID
#define SRV_SEND_ID 0x7A8      // server responds on this CAN ID
#define ISOTP_BUFSIZE 256

static Iso14229Server srv;
static bool ecu_reset_scheduled = false;
static int ecu_reset_timer = 0;
static struct RWDBIData {
    uint8_t d0001;
    int8_t d0002;
    uint16_t d0003;
    int16_t d0004;
} myData = {0};

// mock an ECU reset by resetting the server
void mockECUReset() {
    printf("Resetting ECU\n");
    Iso14229ServerInit(&srv, srv.cfg);
    ecu_reset_scheduled = false;
}

void sessionTimeout() {
    printf("server session timed out!\n");
    mockECUReset();
}

void scheduleReset(int when) {
    ecu_reset_scheduled = true;
    ecu_reset_timer = hostGetms() + when;
    printf("scheduled ECUReset for %d ms from now\n", when);
}

enum Iso14229ResponseCode ECUResetHandler(const struct Iso14229ServerStatus *status,
                                          uint8_t resetType, uint8_t *powerDownTime) {
    (void)status;
    (void)powerDownTime;
    printf("got ECUReset request of type %x\n", resetType);
    /**
     * You'd check to see whether the ECU is allowed to reset
     *
     * if (safeToReset) {
     *    scheduleReset();
     *    return kPositiveResponse;
     * } else {
     *    return kConditionsNotCorrect;
     * }
     *
     */
    scheduleReset(50);
    return kPositiveResponse;
}

enum Iso14229ResponseCode diagnosticSessionControlHandler(const struct Iso14229ServerStatus *status,
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

enum Iso14229ResponseCode securityAccessGenerateSeed(const struct Iso14229ServerStatus *status,
                                                     uint8_t level, const uint8_t *in_data,
                                                     uint16_t in_size, uint8_t *out_data,
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

enum Iso14229ResponseCode securityAccessValidateKey(const struct Iso14229ServerStatus *status,
                                                    uint8_t level, const uint8_t *key,
                                                    uint16_t size) {
    (void)status;
    (void)level;
    (void)key;
    (void)size;
    // doValidation(key, size);
    return kPositiveResponse;
}

enum Iso14229ResponseCode rdbiHandler(const struct Iso14229ServerStatus *status, uint16_t dataId,
                                      const uint8_t **data_location, uint16_t *len) {
    (void)status;
    static const uint8_t msg[] = "I'm a UDS server    ";
    switch (dataId) {
    case 0x0001:
        *data_location = &myData.d0001;
        *len = sizeof(myData.d0001);
        break;
    case 0x0008:
        *data_location = msg;
        *len = sizeof(msg);
        break;
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}
enum Iso14229ResponseCode wdbiHandler(const struct Iso14229ServerStatus *status, uint16_t dataId,
                                      const uint8_t *data, uint16_t len) {
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

int hostSendCANShim(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    printf("send> 0x%03x: ", arbitration_id);
    PRINTHEX(data, size);
    return hostSendCAN(arbitration_id, data, size);
}

// =====================================
// STEP 2: initialize the server
// =====================================

int main(int ac, char **av) {
    // setup the linux CAN socket. This will vary depending on your platform.
    hostSetup(ac, av);

    uint8_t isotpPhysRecvBuf[ISOTP_BUFSIZE];
    uint8_t isotpPhysSendBuf[ISOTP_BUFSIZE];
    uint8_t isotpFuncRecvBuf[ISOTP_BUFSIZE];
    uint8_t isotpFuncSendBuf[ISOTP_BUFSIZE];
    uint8_t udsSendBuf[ISOTP_BUFSIZE];
    uint8_t udsRecvBuf[ISOTP_BUFSIZE];

    IsoTpLink isotpPhysLink;
    IsoTpLink isotpFuncLink;

    const Iso14229ServerConfig cfg = {
        .phys_recv_id = SRV_PHYS_RECV_ID,
        .func_recv_id = SRV_FUNC_RECV_ID,
        .send_id = SRV_SEND_ID,
        .phys_link = &isotpPhysLink,
        .func_link = &isotpFuncLink,
        .receive_buffer = udsRecvBuf,
        .receive_buf_size = sizeof(udsRecvBuf),
        .send_buffer = udsSendBuf,
        .send_buf_size = sizeof(udsSendBuf),
        .userGetms = hostGetms,
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

    /* initialize the ISO-TP links */
    isotp_init_link(&isotpPhysLink, SRV_SEND_ID, isotpPhysSendBuf, sizeof(isotpPhysSendBuf),
                    isotpPhysRecvBuf, sizeof(isotpPhysRecvBuf), hostGetms, hostSendCANShim,
                    hostPrintf);
    isotp_init_link(&isotpFuncLink, SRV_SEND_ID, isotpFuncSendBuf, sizeof(isotpFuncSendBuf),
                    isotpFuncRecvBuf, sizeof(isotpFuncRecvBuf), hostGetms, hostSendCANShim,
                    hostPrintf);

    Iso14229ServerInit(&srv, &cfg);

    // =====================================
    // STEP 3: poll the server
    // =====================================

    while (!g_should_exit) {
        uint32_t arb_id;
        uint8_t data[8];
        uint8_t size;

        Iso14229ServerPoll(&srv);
        if (0 == hostCANRxPoll(&arb_id, data, &size)) {
            printf("recv> 0x%03x: ", arb_id);
            PRINTHEX(data, size);
            iso14229ServerReceiveCAN(&srv, arb_id, data, size);
        }
        if (ecu_reset_scheduled && Iso14229TimeAfter(ecu_reset_timer, hostGetms())) {
            mockECUReset();
        }
        hostmsleep(10);
    }
}