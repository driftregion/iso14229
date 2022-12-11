#include "iso14229.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// ISO-14229-1:2013 Table 2
#define UDS_MAX_DIAGNOSTIC_SERVICES 0x7F

#define UDS_RESPONSE_SID_OF(request_sid) (request_sid + 0x40)
#define UDS_REQUEST_SID_OF(response_sid) (response_sid - 0x40)

#define UDS_NEG_RESP_LEN 3U
#define UDS_0X10_RESP_LEN 6U
#define UDS_0X11_REQ_MIN_LEN 2U
#define UDS_0X11_RESP_BASE_LEN 2U
#define UDS_0X22_RESP_BASE_LEN 1U
#define UDS_0X27_REQ_BASE_LEN 2U
#define UDS_0X27_RESP_BASE_LEN 2U
#define UDS_0X28_REQ_BASE_LEN 3U
#define UDS_0X28_RESP_LEN 2U
#define UDS_0X2E_REQ_BASE_LEN 3U
#define UDS_0X2E_REQ_MIN_LEN 4U
#define UDS_0X2E_RESP_LEN 3U
#define UDS_0X31_REQ_MIN_LEN 4U
#define UDS_0X31_RESP_MIN_LEN 4U
#define UDS_0X34_REQ_BASE_LEN 3U
#define UDS_0X34_RESP_BASE_LEN 2U
#define UDS_0X35_REQ_BASE_LEN 3U
#define UDS_0X36_REQ_BASE_LEN 2U
#define UDS_0X36_RESP_BASE_LEN 2U
#define UDS_0X37_REQ_BASE_LEN 1U
#define UDS_0X37_RESP_BASE_LEN 1U
#define UDS_0X3E_REQ_MIN_LEN 2U
#define UDS_0X3E_RESP_LEN 2U
#define UDS_0X85_REQ_BASE_LEN 2U
#define UDS_0X85_RESP_LEN 2U

enum UDSDiagnosticServiceId {
    kSID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    kSID_ECU_RESET = 0x11,
    kSID_CLEAR_DIAGNOSTIC_INFORMATION = 0x14,
    kSID_READ_DTC_INFORMATION = 0x19,
    kSID_READ_DATA_BY_IDENTIFIER = 0x22,
    kSID_READ_MEMORY_BY_ADDRESS = 0x23,
    kSID_READ_SCALING_DATA_BY_IDENTIFIER = 0x24,
    kSID_SECURITY_ACCESS = 0x27,
    kSID_COMMUNICATION_CONTROL = 0x28,
    kSID_READ_PERIODIC_DATA_BY_IDENTIFIER = 0x2A,
    kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER = 0x2C,
    kSID_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    kSID_INPUT_CONTROL_BY_IDENTIFIER = 0x2F,
    kSID_ROUTINE_CONTROL = 0x31,
    kSID_REQUEST_DOWNLOAD = 0x34,
    kSID_REQUEST_UPLOAD = 0x35,
    kSID_TRANSFER_DATA = 0x36,
    kSID_REQUEST_TRANSFER_EXIT = 0x37,
    kSID_REQUEST_FILE_TRANSFER = 0x38,
    kSID_WRITE_MEMORY_BY_ADDRESS = 0x3D,
    kSID_TESTER_PRESENT = 0x3E,
    kSID_ACCESS_TIMING_PARAMETER = 0x83,
    kSID_SECURED_DATA_TRANSMISSION = 0x84,
    kSID_CONTROL_DTC_SETTING = 0x85,
    kSID_RESPONSE_ON_EVENT = 0x86,
};

// ========================================================================
//                              Transports
// ========================================================================

#if UDS_TP == UDS_TP_CUSTOM
#else
static UDSTpStatus_t tp_poll(UDSTpHandle_t *hdl) {
    assert(hdl);
    UDSTpStatus_t status = 0;
#if UDS_TP == UDS_TP_ISOTP_C
    UDSTpIsoTpC_t *impl = hdl->impl;
    isotp_poll(&impl->phys_link);
    isotp_poll(&impl->func_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= TP_SEND_INPROGRESS;
    }
#elif UDS_TP == UDS_TP_LINUX_SOCKET
#endif
    return status;
}
#endif

#if UDS_TP == UDS_TP_CUSTOM
#else
static ssize_t tp_recv(struct UDSTpHandle *hdl, void *buf, size_t count, UDSTpAddr_t *ta_type) {
    assert(hdl);
    assert(ta_type);
    assert(buf);

#if UDS_TP == UDS_TP_ISOTP_C
    uint16_t size = 0;
    int ret = 0;
    UDSTpIsoTpC_t *impl = hdl->impl;
    struct {
        IsoTpLink *link;
        UDSTpAddr_t ta_type;
    } arr[] = {{&impl->phys_link, kTpAddrTypePhysical}, {&impl->func_link, kTpAddrTypeFunctional}};
    for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
        ret = isotp_receive(arr[i].link, buf, count, &size);
        switch (ret) {
        case ISOTP_RET_OK:
            *ta_type = arr[i].ta_type;
            return size;
        case ISOTP_RET_NO_DATA:
            continue;
        case ISOTP_RET_ERROR:
            return ISOTP_RET_ERROR;
        default:
            return -2;
        }
    }
    return 0;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl->impl;
    int size = 0;
    struct {
        int fd;
        UDSTpAddr_t ta_type;
    } arr[] = {{impl->phys_fd, kTpAddrTypePhysical}, {impl->func_fd, kTpAddrTypeFunctional}};
    for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
        size = read(arr[i].fd, buf, count);
        if (size < 0) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                continue;
            }
            printf("read. fd: %d, %s\n", arr[i].fd, strerror(errno));
        } else {
            *ta_type = arr[i].ta_type;
            return size;
        }
    }
    return 0;
#endif
}
#endif

#if UDS_TP == UDS_TP_CUSTOM
#else
static ssize_t tp_send(struct UDSTpHandle *hdl, const void *buf, size_t count,
                       UDSTpAddr_t ta_type) {
    assert(hdl);
#if UDS_TP == UDS_TP_ISOTP_C
    UDSTpIsoTpC_t *impl = hdl->impl;
    IsoTpLink *link = NULL;
    switch (ta_type) {
    case kTpAddrTypePhysical:
        link = &impl->phys_link;
        break;
    case kTpAddrTypeFunctional:
        link = &impl->func_link;
        break;
    default:
        return -4;
    }
    int send_status = isotp_send(link, buf, count);
    switch (send_status) {
    case ISOTP_RET_OK:
        return count;
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        return send_status;
    }
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl->impl;
    int fd;
    switch (ta_type) {
    case kTpAddrTypePhysical:
        fd = impl->phys_fd;
        break;
    case kTpAddrTypeFunctional:
        fd = impl->func_fd;
        break;
    default:
        return -4;
    }
    int result = write(fd, buf, count);
    if (result < 0) {
        printf("write. fd: %d, errno: %d\n", fd, errno);
        perror("");
    }
    return result;
#endif
}
#endif

#if UDS_TP == UDS_TP_LINUX_SOCKET
static int LinuxSockBind(const char *if_name, uint16_t rxid, uint16_t txid) {
    int fd = 0;
    if ((fd = socket(AF_CAN, SOCK_DGRAM | SOCK_NONBLOCK, CAN_ISOTP)) < 0) {
        perror("Socket");
        return -1;
    }

    struct can_isotp_fc_options fcopts = {
        .bs = 0x10,
        .stmin = 3,
        .wftmax = 0,
    };
    if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_RECV_FC, &fcopts, sizeof(fcopts)) < 0) {
        perror("setsockopt");
        return -1;
    }

    struct ifreq ifr;
    strcpy(ifr.ifr_name, if_name);
    ioctl(fd, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_addr.tp.rx_id = rxid;
    addr.can_addr.tp.tx_id = txid;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Bind: %s %s\n", strerror(errno), if_name);
        return -1;
    }
    printf("opened ISO-TP link fd: %d, rxid: %03x, txid: %03x\n", fd, rxid, txid);
    return fd;
}

static int LinuxSockTpOpen(UDSTpHandle_t *hdl, const char *if_name, uint16_t phys_rxid,
                           uint16_t phys_txid, uint16_t func_rxid, uint16_t func_txid) {
    assert(if_name);
    UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl->impl;
    hdl->recv = tp_recv;
    hdl->send = tp_send;
    hdl->poll = tp_poll;
    impl->phys_fd = LinuxSockBind(if_name, phys_rxid, phys_txid);
    impl->func_fd = LinuxSockBind(if_name, func_rxid, func_txid);
    if (impl->phys_fd < 0 || impl->func_fd < 0) {
        return -1;
    }
    return 0;
}

void LinuxSockTpClose(UDSTpHandle_t *hdl) {
    if (hdl) {
        UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl->impl;
        if (impl) {
            if (close(impl->phys_fd) < 0) {
                perror("failed to close socket");
            }
            if (close(impl->func_fd) < 0) {
                perror("failed to close socket");
            }
        }
    }
}
#endif // #if UDS_TP == UDS_TP_LINUX_SOCKET
// ========================================================================
//                              Common
// ========================================================================

#if UDS_CUSTOM_MILLIS
#else
uint32_t UDSMillis() {
#if UDS_ARCH == UDS_ARCH_UNIX
    struct timeval te;
    gettimeofday(&te, NULL);                                         // get current time
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
    return milliseconds;
#endif
}
#endif

static bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel) {
    securityLevel &= 0x3f;
    return (0 == securityLevel || (0x43 <= securityLevel && securityLevel >= 0x5E) ||
            0x7F == securityLevel);
}

// ========================================================================
//                              Server
// ========================================================================

static inline uint8_t NegativeResponse(UDSServer_t *self, uint8_t response_code) {
    self->send_buf[0] = 0x7F;
    self->send_buf[1] = self->recv_buf[0];
    self->send_buf[2] = response_code;
    self->send_size = UDS_NEG_RESP_LEN;
    return response_code;
}

static inline void NoResponse(UDSServer_t *self) { self->send_size = 0; }

static uint8_t _0x10_DiagnosticSessionControl(UDSServer_t *self) {
    if (self->recv_size < 1) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    enum UDSDiagnosticSessionType sessType = self->recv_buf[1] & 0x4F;

    if (NULL == self->fn) {
        return NegativeResponse(self, kServiceNotSupported);
    }

    UDSDiagSessCtrlArgs_t args = {
        .type = sessType,
        .p2_ms = self->p2_ms,
        .p2_star_ms = self->p2_star_ms,
    };

    uint8_t err = self->fn(self, UDS_SRV_EVT_DiagSessCtrl, &args);

    if (kPositiveResponse != err) {
        return NegativeResponse(self, err);
    }

    self->sessionType = sessType;
    self->p2_ms = args.p2_ms;
    self->p2_star_ms = args.p2_star_ms;

    switch (sessType) {
    case kDefaultSession:
        break;
    case kProgrammingSession:
    case kExtendedDiagnostic:
    default:
        self->s3_session_timeout_timer = UDSMillis() + self->s3_ms;
        break;
    }

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_DIAGNOSTIC_SESSION_CONTROL);
    self->send_buf[1] = sessType;

    // UDS-1-2013: Table 29
    // resolution: 1ms
    self->send_buf[2] = self->p2_ms >> 8;
    self->send_buf[3] = self->p2_ms;

    // resolution: 10ms
    self->send_buf[4] = (self->p2_star_ms / 10) >> 8;
    self->send_buf[5] = self->p2_star_ms / 10;

    self->send_size = UDS_0X10_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x11_ECUReset(UDSServer_t *self) {
    uint8_t resetType = self->recv_buf[1] & 0x3F;

    if (self->recv_size < UDS_0X11_REQ_MIN_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    if (NULL == self->fn) {
        return NegativeResponse(self, kServiceNotSupported);
    }

    UDSECUResetArgs_t args = {
        .type = resetType,
        .powerDownTime = 0,
    };

    uint8_t err = self->fn(self, UDS_SRV_EVT_EcuReset, &args);

    if (kPositiveResponse == err) {
        self->notReadyToReceive = true;
        self->ecuResetScheduled = true;
    } else {
        return NegativeResponse(self, err);
    }

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ECU_RESET);
    self->send_buf[1] = resetType;

    if (kEnableRapidPowerShutDown == resetType) {
        self->send_buf[2] = args.powerDownTime;
        self->send_size = UDS_0X11_RESP_BASE_LEN + 1;
    } else {
        self->send_size = UDS_0X11_RESP_BASE_LEN;
    }
    return kPositiveResponse;
}

static uint8_t safe_copy(UDSServer_t *srv, const void *src, uint16_t count) {
    if (count <= srv->send_buf_size - srv->send_size) {
        memmove(srv->send_buf + srv->send_size, src, count);
        srv->send_size += count;
        return kPositiveResponse;
    }
    return kResponseTooLong;
}

static uint8_t _0x22_ReadDataByIdentifier(UDSServer_t *self) {
    uint8_t numDIDs;
    uint16_t dataId = 0;
    uint8_t ret = kPositiveResponse;
    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_DATA_BY_IDENTIFIER);
    self->send_size = 1;

    if (NULL == self->fn) {
        return NegativeResponse(self, kServiceNotSupported);
    }

    if (0 != (self->recv_size - 1) % sizeof(uint16_t)) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    numDIDs = self->recv_size / sizeof(uint16_t);

    if (0 == numDIDs) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (int did = 0; did < numDIDs; did++) {
        uint16_t idx = 1 + did * 2;
        dataId = (self->recv_buf[idx] << 8) + self->recv_buf[idx + 1];

        if (self->send_size + 3 > self->send_buf_size) {
            return NegativeResponse(self, kResponseTooLong);
        }
        uint8_t *copylocation = self->send_buf + self->send_size;
        copylocation[0] = dataId >> 8;
        copylocation[1] = dataId;
        self->send_size += 2;

        UDSRDBIArgs_t args = {
            .dataId = dataId,
            .copy = safe_copy,
        };

        ret = self->fn(self, UDS_SRV_EVT_ReadDataByIdent, &args);

        if (kPositiveResponse != ret) {
            return NegativeResponse(self, ret);
        }
    }
    return kPositiveResponse;
}

static uint8_t _0x27_SecurityAccess(UDSServer_t *self) {
    uint8_t subFunction = self->recv_buf[1];
    uint8_t response = kPositiveResponse;

    if (UDSSecurityAccessLevelIsReserved(subFunction)) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    if (NULL == self->fn) {
        return NegativeResponse(self, kServiceNotSupported);
    }

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_SECURITY_ACCESS);
    self->send_buf[1] = subFunction;
    self->send_size = UDS_0X27_RESP_BASE_LEN;

    // Even: sendKey
    if (0 == subFunction % 2) {
        uint8_t requestedLevel = subFunction - 1;
        UDSSecAccessValidateKeyArgs_t args = {
            .level = requestedLevel,
            .key = &self->recv_buf[UDS_0X27_REQ_BASE_LEN],
            .key_len = self->recv_size - UDS_0X27_REQ_BASE_LEN,
        };

        response = self->fn(self, UDS_SRV_EVT_SecAccessValidateKey, &args);

        if (kPositiveResponse != response) {
            return NegativeResponse(self, response);
        }

        // "requestSeed = 0x01" identifies a fixed relationship between
        // "requestSeed = 0x01" and "sendKey = 0x02"
        // "requestSeed = 0x03" identifies a fixed relationship between
        // "requestSeed = 0x03" and "sendKey = 0x04"
        self->securityLevel = requestedLevel;
        self->send_size = UDS_0X27_RESP_BASE_LEN;
        return kPositiveResponse;
    }

    // Odd: requestSeed
    else {
        /* If a server supports security, but the requested security level is already unlocked when
        a SecurityAccess ‘requestSeed’ message is received, that server shall respond with a
        SecurityAccess ‘requestSeed’ positive response message service with a seed value equal to
        zero (0). The server shall never send an all zero seed for a given security level that is
        currently locked. The client shall use this method to determine if a server is locked for a
        particular security level by checking for a non-zero seed.
        */
        if (subFunction == self->securityLevel) {
            // Table 52 sends a response of length 2. Use a preprocessor define if this needs
            // customizing by the user.
            const uint8_t already_unlocked[] = {0x00, 0x00};
            return safe_copy(self, already_unlocked, sizeof(already_unlocked));
        } else {
            UDSSecAccessGenerateSeedArgs_t args = {
                .level = subFunction,
                .in_data = &self->recv_buf[UDS_0X27_REQ_BASE_LEN],
                .in_size = self->recv_size - UDS_0X27_REQ_BASE_LEN,
                .copySeed = safe_copy,
            };

            response = self->fn(self, UDS_SRV_EVT_SecAccessGenerateSeed, &args);

            if (kPositiveResponse != response) {
                return NegativeResponse(self, response);
            }

            if (self->send_size <= UDS_0X27_RESP_BASE_LEN) { // no data was copied
                return NegativeResponse(self, kGeneralProgrammingFailure);
            }
            return kPositiveResponse;
        }
    }
    return NegativeResponse(self, kGeneralProgrammingFailure);
}

static uint8_t _0x28_CommunicationControl(UDSServer_t *self) {
    uint8_t controlType = self->recv_buf[1] & 0x7F;
    uint8_t communicationType = self->recv_buf[2];

    if (self->recv_size < UDS_0X28_REQ_BASE_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    UDSCommCtrlArgs_t args = {
        .ctrlType = controlType,
        .commType = communicationType,
    };

    uint8_t err = self->fn(self, UDS_SRV_EVT_CommCtrl, &args);
    if (kPositiveResponse != err) {
        return NegativeResponse(self, err);
    }

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_COMMUNICATION_CONTROL);
    self->send_buf[1] = controlType;
    self->send_size = UDS_0X28_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x2E_WriteDataByIdentifier(UDSServer_t *self) {
    uint16_t dataLen = 0;
    uint16_t dataId = 0;
    uint8_t err = kPositiveResponse;

    /* UDS-1 2013 Figure 21 Key 1 */
    if (self->recv_size < UDS_0X2E_REQ_MIN_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    dataId = (self->recv_buf[1] << 8) + self->recv_buf[2];
    dataLen = self->recv_size - UDS_0X2E_REQ_BASE_LEN;

    UDSWDBIArgs_t args = {
        .dataId = dataId,
        .data = &self->recv_buf[UDS_0X2E_REQ_BASE_LEN],
        .len = dataLen,
    };

    err = self->fn(self, UDS_SRV_EVT_WriteDataByIdent, &args);
    if (kPositiveResponse != err) {
        return NegativeResponse(self, err);
    }

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_WRITE_DATA_BY_IDENTIFIER);
    self->send_buf[1] = dataId >> 8;
    self->send_buf[2] = dataId;
    self->send_size = UDS_0X2E_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x31_RoutineControl(UDSServer_t *self) {
    uint8_t err = kPositiveResponse;
    if (self->recv_size < UDS_0X31_REQ_MIN_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t routineControlType = self->recv_buf[1] & 0x7F;
    uint16_t routineIdentifier = (self->recv_buf[2] << 8) + self->recv_buf[3];

    UDSRoutineCtrlArgs_t args = {
        .ctrlType = routineControlType,
        .id = routineIdentifier,
        .optionRecord = &self->recv_buf[UDS_0X31_REQ_MIN_LEN],
        .optionRecordLength = self->recv_size - UDS_0X31_REQ_MIN_LEN,
        .copyStatusRecord = safe_copy,
    };

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL);
    self->send_buf[1] = routineControlType;
    self->send_buf[2] = routineIdentifier >> 8;
    self->send_buf[3] = routineIdentifier;
    self->send_size = UDS_0X31_RESP_MIN_LEN;

    switch (routineControlType) {
    case kStartRoutine:
    case kStopRoutine:
    case kRequestRoutineResults:
        err = self->fn(self, UDS_SRV_EVT_RoutineCtrl, &args);
        if (kPositiveResponse != err) {
            return NegativeResponse(self, err);
        }
        break;
    default:
        return NegativeResponse(self, kRequestOutOfRange);
    }
    return kPositiveResponse;
}

static void ResetTransfer(UDSServer_t *srv) {
    assert(srv);
    srv->xferBlockSequenceCounter = 1;
    srv->xferByteCounter = 0;
    srv->xferTotalBytes = 0;
    srv->xferIsActive = false;
}

static uint8_t _0x34_RequestDownload(UDSServer_t *self) {
    uint8_t err;
    size_t memoryAddress = 0;
    size_t memorySize = 0;

    if (self->xferIsActive) {
        return NegativeResponse(self, kConditionsNotCorrect);
    }

    if (self->recv_size < UDS_0X34_REQ_BASE_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t dataFormatIdentifier = self->recv_buf[1];
    uint8_t memorySizeLength = (self->recv_buf[2] & 0xF0) >> 4;
    uint8_t memoryAddressLength = self->recv_buf[2] & 0x0F;

    if (memorySizeLength == 0 || memorySizeLength > sizeof(memorySize)) {
        return NegativeResponse(self, kRequestOutOfRange);
    }

    if (memoryAddressLength == 0 || memoryAddressLength > sizeof(memoryAddress)) {
        return NegativeResponse(self, kRequestOutOfRange);
    }

    if (self->recv_size < UDS_0X34_REQ_BASE_LEN + memorySizeLength + memoryAddressLength) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (int byteIdx = 0; byteIdx < memoryAddressLength; byteIdx++) {
        uint8_t byte = self->recv_buf[UDS_0X34_REQ_BASE_LEN + byteIdx];
        uint8_t shiftBytes = memoryAddressLength - 1 - byteIdx;
        memoryAddress |= byte << (8 * shiftBytes);
    }

    for (int byteIdx = 0; byteIdx < memorySizeLength; byteIdx++) {
        uint8_t byte = self->recv_buf[UDS_0X34_REQ_BASE_LEN + memoryAddressLength + byteIdx];
        uint8_t shiftBytes = memorySizeLength - 1 - byteIdx;
        memorySize |= byte << (8 * shiftBytes);
    }

    UDSRequestDownloadArgs_t args = {
        .addr = (void *)memoryAddress,
        .size = memorySize,
        .dataFormatIdentifier = dataFormatIdentifier,
        .maxNumberOfBlockLength = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH,
    };

    err = self->fn(self, UDS_SRV_EVT_RequestDownload, &args);

    if (args.maxNumberOfBlockLength < 3) {
        UDS_DBG_PRINT("ERROR: maxNumberOfBlockLength too short");
        return NegativeResponse(self, kGeneralProgrammingFailure);
    }

    if (kPositiveResponse != err) {
        return NegativeResponse(self, err);
    }

    ResetTransfer(self);
    self->xferIsActive = true;
    self->xferTotalBytes = memorySize;

    // ISO-14229-1:2013 Table 401:
    uint8_t lengthFormatIdentifier = sizeof(args.maxNumberOfBlockLength) << 4;

    /* ISO-14229-1:2013 Table 396: maxNumberOfBlockLength
    This parameter is used by the requestDownload positive response message to
    inform the client how many data bytes (maxNumberOfBlockLength) to include in
    each TransferData request message from the client. This length reflects the
    complete message length, including the service identifier and the
    data-parameters present in the TransferData request message.
    */
    if (args.maxNumberOfBlockLength > UDS_TP_MTU) {
        args.maxNumberOfBlockLength = UDS_TP_MTU;
    }

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_DOWNLOAD);
    self->send_buf[1] = lengthFormatIdentifier;
    for (uint8_t idx = 0; idx < sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = sizeof(args.maxNumberOfBlockLength) - 1 - idx;
        uint8_t byte = args.maxNumberOfBlockLength >> (shiftBytes * 8);
        self->send_buf[UDS_0X34_RESP_BASE_LEN + idx] = byte;
    }
    self->send_size = UDS_0X34_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength);
    return kPositiveResponse;
}

static uint8_t _0x36_TransferData(UDSServer_t *self) {
    uint8_t err = kPositiveResponse;
    uint16_t request_data_len = self->recv_size - UDS_0X36_REQ_BASE_LEN;

    if (self->recv_size < UDS_0X36_REQ_BASE_LEN) {
        err = kIncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    uint8_t blockSequenceCounter = self->recv_buf[1];

    if (!self->xferIsActive) {
        return NegativeResponse(self, kUploadDownloadNotAccepted);
    }

    if (!self->RCRRP) {
        if (blockSequenceCounter != self->xferBlockSequenceCounter) {
            err = kRequestSequenceError;
            goto fail;
        } else {
            self->xferBlockSequenceCounter++;
        }
    }

    if (self->xferByteCounter + request_data_len > self->xferTotalBytes) {
        err = kTransferDataSuspended;
        goto fail;
    }

    UDSTransferDataArgs_t args = {
        .req = &self->recv_buf[UDS_0X36_REQ_BASE_LEN],
        .req_len = self->recv_size - UDS_0X36_REQ_BASE_LEN,
        .copyResponse = safe_copy,
    };

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TRANSFER_DATA);
    self->send_buf[1] = blockSequenceCounter;
    self->send_size = UDS_0X36_RESP_BASE_LEN;

    err = self->fn(self, UDS_SRV_EVT_TransferData, &args);

    switch (err) {
    case kPositiveResponse:
        self->xferByteCounter += request_data_len;
        return kPositiveResponse;
    case kRequestCorrectlyReceived_ResponsePending:
        return NegativeResponse(self, kRequestCorrectlyReceived_ResponsePending);
    default:
        err = kGeneralProgrammingFailure;
        goto fail;
    }

fail:
    ResetTransfer(self);
    return NegativeResponse(self, err);
}

static uint8_t _0x37_RequestTransferExit(UDSServer_t *self) {
    uint8_t err = kPositiveResponse;

    if (!self->xferIsActive) {
        return NegativeResponse(self, kUploadDownloadNotAccepted);
    }

    ResetTransfer(self);

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_TRANSFER_EXIT);
    self->send_size = UDS_0X37_RESP_BASE_LEN;

    UDSRequestTransferExitArgs_t args = {
        .req = &self->recv_buf[UDS_0X37_REQ_BASE_LEN],
        .req_len = self->recv_size - UDS_0X37_REQ_BASE_LEN,
        .copyResponse = safe_copy,
    };

    err = self->fn(self, UDS_SRV_EVT_RequestTransferExit, &args);

    if (err != kPositiveResponse) {
        return NegativeResponse(self, err);
    }

    return kPositiveResponse;
}

static uint8_t _0x3E_TesterPresent(UDSServer_t *self) {
    if (self->recv_size < UDS_0X3E_REQ_MIN_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }
    self->s3_session_timeout_timer = UDSMillis() + self->s3_ms;
    uint8_t zeroSubFunction = self->recv_buf[1];
    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TESTER_PRESENT);
    self->send_buf[1] = zeroSubFunction & 0x3F;
    self->send_size = UDS_0X3E_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x85_ControlDTCSetting(UDSServer_t *self) {
    (void)self;
    if (self->recv_size < UDS_0X85_REQ_BASE_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }
    uint8_t dtcSettingType = self->recv_buf[1] & 0x3F;

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_CONTROL_DTC_SETTING);
    self->send_buf[1] = dtcSettingType;
    self->send_size = UDS_0X85_RESP_LEN;
    return kPositiveResponse;
}

/**
 * @brief Get the internal service handler matching the given SID.
 * @param sid
 * @return pointer to UDSService or NULL if no match
 */
static UDSService getServiceForSID(uint8_t sid) {
    switch (sid) {
    case kSID_DIAGNOSTIC_SESSION_CONTROL:
        return _0x10_DiagnosticSessionControl;
    case kSID_ECU_RESET:
        return _0x11_ECUReset;
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
        return NULL;
    case kSID_READ_DTC_INFORMATION:
        return NULL;
    case kSID_READ_DATA_BY_IDENTIFIER:
        return _0x22_ReadDataByIdentifier;
    case kSID_READ_MEMORY_BY_ADDRESS:
        return NULL;
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
        return NULL;
    case kSID_SECURITY_ACCESS:
        return _0x27_SecurityAccess;
    case kSID_COMMUNICATION_CONTROL:
        return _0x28_CommunicationControl;
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
        return NULL;
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
        return NULL;
    case kSID_WRITE_DATA_BY_IDENTIFIER:
        return _0x2E_WriteDataByIdentifier;
    case kSID_INPUT_CONTROL_BY_IDENTIFIER:
        return NULL;
    case kSID_ROUTINE_CONTROL:
        return _0x31_RoutineControl;
    case kSID_REQUEST_DOWNLOAD:
        return _0x34_RequestDownload;
    case kSID_REQUEST_UPLOAD:
        return NULL;
    case kSID_TRANSFER_DATA:
        return _0x36_TransferData;
    case kSID_REQUEST_TRANSFER_EXIT:
        return _0x37_RequestTransferExit;
    case kSID_REQUEST_FILE_TRANSFER:
        return NULL;
    case kSID_WRITE_MEMORY_BY_ADDRESS:
        return NULL;
    case kSID_TESTER_PRESENT:
        return _0x3E_TesterPresent;
    case kSID_ACCESS_TIMING_PARAMETER:
        return NULL;
    case kSID_SECURED_DATA_TRANSMISSION:
        return NULL;
    case kSID_CONTROL_DTC_SETTING:
        return _0x85_ControlDTCSetting;
    case kSID_RESPONSE_ON_EVENT:
        return NULL;
    default:
        UDS_DBG_PRINT("no handler for request SID %x.\n", sid);
        return NULL;
    }
}

/**
 * @brief Call the service if it exists, modifying the response if the spec calls for it.
 * @note see UDS-1 2013 7.5.5 Pseudo code example of server response behavior
 *
 * @param self
 * @param addressingScheme
 */
static uint8_t evaluateServiceResponse(UDSServer_t *self, const uint8_t addressingScheme) {
    uint8_t response = kPositiveResponse;
    bool suppressResponse = false;
    uint8_t sid = self->recv_buf[0];
    UDSService service = getServiceForSID(sid);

    if (NULL == service || NULL == self->fn) {
        return NegativeResponse(self, kServiceNotSupported);
    }

    switch (sid) {
    /* CASE Service_with_sub-function */
    /* test if service with sub-function is supported */
    case kSID_DIAGNOSTIC_SESSION_CONTROL:
    case kSID_ECU_RESET:
    case kSID_READ_DTC_INFORMATION:
    case kSID_SECURITY_ACCESS:
    case kSID_COMMUNICATION_CONTROL:
    case kSID_ROUTINE_CONTROL:
    case kSID_TESTER_PRESENT:
    case kSID_ACCESS_TIMING_PARAMETER:
    case kSID_SECURED_DATA_TRANSMISSION:
    case kSID_CONTROL_DTC_SETTING:
    case kSID_RESPONSE_ON_EVENT: {

        /* check minimum length of message with sub-function */
        if (self->recv_size >= 2) {
            /* get sub-function parameter value without bit 7 */
            // switch (ctx->req.as.raw[0] & 0x7F) {

            // }
            // Let the service callback determine whether or not the sub-function parameter value is
            // supported
            response = service(self);
        } else {
            /* NRC 0x13: incorrectMessageLengthOrInvalidFormat */
            response = kIncorrectMessageLengthOrInvalidFormat;
        }

        bool suppressPosRspMsgIndicationBit = self->recv_buf[1] & 0x80;

        /* test if positive response is required and if responseCode is positive 0x00 */
        if ((suppressPosRspMsgIndicationBit) && (response == kPositiveResponse) &&
            (
                // TODO: *not yet a NRC 0x78 response sent*
                true)) {
            suppressResponse = true;
        } else {
            suppressResponse = false;
        }
        break;
    }

    /* CASE Service_without_sub-function */
    /* test if service without sub-function is supported */
    case kSID_READ_DATA_BY_IDENTIFIER:
    case kSID_READ_MEMORY_BY_ADDRESS:
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
    case kSID_WRITE_DATA_BY_IDENTIFIER:
    case kSID_REQUEST_DOWNLOAD:
    case kSID_REQUEST_UPLOAD:
    case kSID_TRANSFER_DATA:
    case kSID_REQUEST_TRANSFER_EXIT:
    case kSID_REQUEST_FILE_TRANSFER:
    case kSID_WRITE_MEMORY_BY_ADDRESS:
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
    case kSID_INPUT_CONTROL_BY_IDENTIFIER: {
        response = service(self);
        break;
    }

    default: {
        response = kServiceNotSupported;
        break;
    }
    }

    if ((kTpAddrTypeFunctional == addressingScheme) &&
        ((kServiceNotSupported == response) || (kSubFunctionNotSupported == response) ||
         (kServiceNotSupportedInActiveSession == response) ||
         (kSubFunctionNotSupportedInActiveSession == response) ||
         (kRequestOutOfRange == response)) &&
        (
            // TODO: *not yet a NRC 0x78 response sent*
            true)) {
        suppressResponse = true; /* Suppress negative response message */
        NoResponse(self);
    } else {
        if (suppressResponse) { /* Suppress positive response message */
            NoResponse(self);
        } else { /* send negative or positive response */
            ;
        }
    }
    return response;
}

/**
 * @brief Process the data on this link
 *
 * @param self
 * @param link transport handle
 * @param addressingScheme
 */
static void ProcessLink(UDSServer_t *self, const UDSTpAddr_t ta_type) {

    uint8_t response = evaluateServiceResponse(self, ta_type);

    if (kRequestCorrectlyReceived_ResponsePending == response) {
        self->RCRRP = true;
        self->notReadyToReceive = true;
    } else {
        self->RCRRP = false;
    }

    if (self->send_size) {
        int result = self->tp->send(self->tp, self->send_buf, self->send_size, ta_type);
        assert(result == self->send_size); // how should it be handled if send fails?
    }
}

// ========================================================================
//                             Public Functions
// ========================================================================

/**
 * @brief \~chinese 初始化服务器 \~english Initialize the server
 *
 * @param self
 * @param cfg
 * @return int
 */
UDSErr_t UDSServerInit(UDSServer_t *self, const UDSServerConfig_t *cfg) {
    assert(self);
    assert(cfg);
    memset(self, 0, sizeof(UDSServer_t));
    self->recv_buf_size = sizeof(self->recv_buf);
    self->send_buf_size = sizeof(self->send_buf);
    self->p2_ms = UDS_SERVER_DEFAULT_P2_MS;
    self->p2_star_ms = UDS_SERVER_DEFAULT_P2_STAR_MS;
    self->s3_ms = UDS_SERVER_DEFAULT_S3_MS;
    self->fn = cfg->fn;
    self->sessionType = kDefaultSession;

    // Initialize p2_timer to an already past time, otherwise the server's
    // response to incoming messages will be delayed.
    self->p2_timer = UDSMillis() - self->p2_ms;

    // Set the session timeout for s3 milliseconds from now.
    self->s3_session_timeout_timer = UDSMillis() + self->s3_ms;

#if UDS_TP == UDS_TP_CUSTOM
    assert(cfg->tp);
    assert(cfg->tp->recv);
    assert(cfg->tp->send);
    self->tp = cfg->tp;
#elif UDS_TP == UDS_TP_ISOTP_C
    assert(cfg->phys_send_id != cfg->func_recv_id && cfg->func_recv_id != cfg->phys_recv_id);
    UDSTpIsoTpC_t *impl = &self->tp_impl;
    isotp_init_link(&impl->phys_link, cfg->phys_send_id, self->send_buf, self->send_buf_size,
                    self->recv_buf, self->recv_buf_size);
    isotp_init_link(&impl->func_link, cfg->phys_send_id, impl->func_send_buf,
                    sizeof(impl->func_send_buf), impl->func_recv_buf, sizeof(impl->func_recv_buf));
    self->_tp_hdl.poll = tp_poll;
    self->_tp_hdl.send = tp_send;
    self->_tp_hdl.recv = tp_recv;
    self->_tp_hdl.impl = &self->tp_impl;
    self->tp = &self->_tp_hdl;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    self->tp = &self->_tp_hdl;
    self->tp->impl = &self->tp_impl;
    if (LinuxSockTpOpen(self->tp, cfg->if_name, cfg->phys_recv_id, cfg->phys_send_id,
                        cfg->func_recv_id, cfg->phys_send_id)) {
        return UDS_ERR;
    }
#endif
    return UDS_OK;
}

void UDSServerDeInit(UDSServer_t *self) {
#if UDS_TP == UDS_TP_LINUX_SOCKET
    LinuxSockTpClose(self->tp);
#endif
}

void UDSServerPoll(UDSServer_t *self) {
    // UDS-1-2013 Figure 38: Session Timeout (S3)
    if (kDefaultSession != self->sessionType &&
        UDSTimeAfter(UDSMillis(), self->s3_session_timeout_timer)) {
        self->fn(self, UDS_SRV_EVT_SessionTimeout, NULL);
    }

    UDSTpStatus_t tp_status = self->tp->poll(self->tp);
    if (tp_status & TP_SEND_INPROGRESS) {
        return;
    }

    // If the user service handler responded RCRRP and the send link is now idle,
    // the response has been sent and the long-running service can now be called.
    if (self->RCRRP) {
        ProcessLink(self, kTpAddrTypePhysical);
        self->notReadyToReceive = self->RCRRP;
        return;
    }

    if (self->notReadyToReceive) {
        return;
    }

    // new data may be processed only after p2 has elapsed
    int size = 0;
    UDSTpAddr_t ta_type = kTpAddrTypePhysical;
    if (UDSTimeAfter(UDSMillis(), self->p2_timer)) {
        size = self->tp->recv(self->tp, self->recv_buf, self->recv_buf_size, &ta_type);
        assert(size >= 0); // what to do if recv fails?
        if (size > 0) {
            printf("Received %d bytes on link %d\n", size, ta_type);
            self->recv_size = size;
            ProcessLink(self, ta_type);
            self->p2_timer = UDSMillis() + self->p2_ms;
        }
    }
}

// ========================================================================
//                              Client
// ========================================================================

static void clearRequestContext(UDSClient_t *client) {
    assert(client);
    assert(client->tp);
    memset(client->recv_buf, 0, client->recv_buf_size);
    memset(client->send_buf, 0, client->send_buf_size);
    client->recv_size = 0;
    client->send_size = 0;
    client->state = kRequestStateIdle;
    client->err = kUDS_CLIENT_OK;
}

UDSErr_t UDSClientInit(UDSClient_t *client, const UDSClientConfig_t *cfg) {
    assert(client);
    assert(cfg);
    memset(client, 0, sizeof(*client));

    client->p2_ms = UDS_CLIENT_DEFAULT_P2_MS;
    client->p2_star_ms = UDS_CLIENT_DEFAULT_P2_STAR_MS;
    client->recv_buf_size = sizeof(client->recv_buf);
    client->send_buf_size = sizeof(client->send_buf);

#if UDS_TP == UDS_TP_CUSTOM
    assert(cfg->tp);
    assert(cfg->tp->recv);
    assert(cfg->tp->send);
    client->tp = cfg->tp;
#elif UDS_TP == UDS_TP_ISOTP_C
    assert(cfg->phys_recv_id != cfg->func_send_id && cfg->func_send_id != cfg->phys_send_id);
    UDSTpIsoTpC_t *impl = &client->tp_impl;
    isotp_init_link(&impl->phys_link, cfg->phys_send_id, client->send_buf, client->send_buf_size,
                    client->recv_buf, client->recv_buf_size);
    isotp_init_link(&impl->func_link, cfg->func_send_id, impl->func_send_buf,
                    sizeof(impl->func_send_buf), impl->func_recv_buf, sizeof(impl->func_recv_buf));
    client->_tp_hdl.poll = tp_poll;
    client->_tp_hdl.send = tp_send;
    client->_tp_hdl.recv = tp_recv;
    client->_tp_hdl.impl = &client->tp_impl;
    client->tp = &client->_tp_hdl;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    client->tp = &client->_tp_hdl;
    client->tp->impl = &client->tp_impl;
    if (LinuxSockTpOpen(client->tp, cfg->if_name, cfg->phys_recv_id, cfg->phys_send_id,
                        cfg->phys_recv_id, cfg->func_send_id)) {
        return UDS_ERR;
    }
    assert(client->tp);
#endif

    clearRequestContext(client);
    return UDS_OK;
}

void UDSClientDeInit(UDSClient_t *client) {
#if UDS_TP == UDS_TP_LINUX_SOCKET
    LinuxSockTpClose(client->tp);
#endif
}

static void changeState(UDSClient_t *client, enum UDSClientRequestState state) {
    // printf("client state: %d -> %d\n", client->state, state);
    client->state = state;
}

static UDSClientError_t _SendRequest(UDSClient_t *client) {
    client->_options_copy = client->options;

    if (client->_options_copy & SUPPRESS_POS_RESP) {
        // UDS-1:2013 8.2.2 Table 11
        client->send_buf[1] |= 0x80;
    }

    changeState(client, kRequestStateSending);
    UDSClientPoll(client);
    return kUDS_CLIENT_OK;
}

#define PRE_REQUEST_CHECK()                                                                        \
    if (client->err)                                                                               \
        return client->err;                                                                        \
    if (kRequestStateIdle != client->state) {                                                      \
        client->err = kUDS_CLIENT_ERR_REQ_NOT_SENT_SEND_IN_PROGRESS;                               \
        return client->err;                                                                        \
    }                                                                                              \
    clearRequestContext(client);

UDSClientError_t UDSSendECUReset(UDSClient_t *client, UDSECUReset_t type) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_ECU_RESET;
    client->send_buf[1] = type;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSClientError_t UDSSendDiagSessCtrl(UDSClient_t *client, enum UDSDiagnosticSessionType mode) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_DIAGNOSTIC_SESSION_CONTROL;
    client->send_buf[1] = mode;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSClientError_t UDSSendCommCtrl(UDSClient_t *client, enum UDSCommunicationControlType ctrl,
                                 enum UDSCommunicationType comm) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_COMMUNICATION_CONTROL;
    client->send_buf[1] = ctrl;
    client->send_buf[2] = comm;
    client->send_size = 3;
    return _SendRequest(client);
}

UDSClientError_t UDSSendTesterPresent(UDSClient_t *client) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_TESTER_PRESENT;
    client->send_buf[1] = 0;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSClientError_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                             const uint16_t numDataIdentifiers) {
    PRE_REQUEST_CHECK();
    assert(didList);
    assert(numDataIdentifiers);
    client->send_buf[0] = kSID_READ_DATA_BY_IDENTIFIER;
    for (int i = 0; i < numDataIdentifiers; i++) {
        uint16_t offset = 1 + sizeof(uint16_t) * i;
        if (offset + 2 > client->send_buf_size) {
            return kUDS_CLIENT_ERR_REQ_NOT_SENT_INVALID_ARGS;
        }
        (client->send_buf + offset)[0] = (didList[i] & 0xFF00) >> 8;
        (client->send_buf + offset)[1] = (didList[i] & 0xFF);
    }
    client->send_size = 1 + (numDataIdentifiers * sizeof(uint16_t));
    return _SendRequest(client);
}

UDSClientError_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                             uint16_t size) {
    PRE_REQUEST_CHECK();
    assert(data);
    assert(size);
    client->send_buf[0] = kSID_WRITE_DATA_BY_IDENTIFIER;
    if (client->send_buf_size <= 3 || size > client->send_buf_size - 3) {
        return kUDS_CLIENT_ERR_REQ_NOT_SENT_BUF_TOO_SMALL;
    }
    client->send_buf[1] = (dataIdentifier & 0xFF00) >> 8;
    client->send_buf[2] = (dataIdentifier & 0xFF);
    memmove(&client->send_buf[3], data, size);
    client->send_size = 3 + size;
    return _SendRequest(client);
}

/**
 * @brief RoutineControl
 *
 * @param client
 * @param type
 * @param routineIdentifier
 * @param data
 * @param size
 * @return UDSClientError_t
 * @addtogroup routineControl_0x31
 */
UDSClientError_t UDSSendRoutineCtrl(UDSClient_t *client, enum RoutineControlType type,
                                    uint16_t routineIdentifier, const uint8_t *data,
                                    uint16_t size) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_ROUTINE_CONTROL;
    client->send_buf[1] = type;
    client->send_buf[2] = routineIdentifier >> 8;
    client->send_buf[3] = routineIdentifier;
    if (size) {
        assert(data);
        if (size > client->send_buf_size - UDS_0X31_REQ_MIN_LEN) {
            return kUDS_CLIENT_ERR_REQ_NOT_SENT_BUF_TOO_SMALL;
        }
        memmove(&client->send_buf[UDS_0X31_REQ_MIN_LEN], data, size);
    } else {
        assert(NULL == data);
    }
    client->send_size = UDS_0X31_REQ_MIN_LEN + size;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param dataFormatIdentifier
 * @param addressAndLengthFormatIdentifier
 * @param memoryAddress
 * @param memorySize
 * @return UDSClientError_t
 * @addtogroup requestDownload_0x34
 */
UDSClientError_t UDSSendRequestDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                        uint8_t addressAndLengthFormatIdentifier,
                                        size_t memoryAddress, size_t memorySize) {
    PRE_REQUEST_CHECK();
    uint8_t numMemorySizeBytes = (addressAndLengthFormatIdentifier & 0xF0) >> 4;
    uint8_t numMemoryAddressBytes = addressAndLengthFormatIdentifier & 0x0F;

    client->send_buf[0] = kSID_REQUEST_DOWNLOAD;
    client->send_buf[1] = dataFormatIdentifier;
    client->send_buf[2] = addressAndLengthFormatIdentifier;

    uint8_t *ptr = &client->send_buf[UDS_0X34_REQ_BASE_LEN];

    for (int i = numMemoryAddressBytes - 1; i >= 0; i--) {
        *ptr = (memoryAddress & (0xFF << (8 * i))) >> (8 * i);
        ptr++;
    }

    for (int i = numMemorySizeBytes - 1; i >= 0; i--) {
        *ptr = (memorySize & (0xFF << (8 * i))) >> (8 * i);
        ptr++;
    }

    client->send_size = UDS_0X34_REQ_BASE_LEN + numMemoryAddressBytes + numMemorySizeBytes;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param dataFormatIdentifier
 * @param addressAndLengthFormatIdentifier
 * @param memoryAddress
 * @param memorySize
 * @return UDSClientError_t
 * @addtogroup requestDownload_0x35
 */
UDSClientError_t UDSSendRequestUpload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                      uint8_t addressAndLengthFormatIdentifier,
                                      size_t memoryAddress, size_t memorySize) {
    PRE_REQUEST_CHECK();
    uint8_t numMemorySizeBytes = (addressAndLengthFormatIdentifier & 0xF0) >> 4;
    uint8_t numMemoryAddressBytes = addressAndLengthFormatIdentifier & 0x0F;

    client->send_buf[0] = kSID_REQUEST_UPLOAD;
    client->send_buf[1] = dataFormatIdentifier;
    client->send_buf[2] = addressAndLengthFormatIdentifier;

    uint8_t *ptr = &client->send_buf[UDS_0X35_REQ_BASE_LEN];

    for (int i = numMemoryAddressBytes - 1; i >= 0; i--) {
        *ptr = (memoryAddress & (0xFF << (8 * i))) >> (8 * i);
        ptr++;
    }

    for (int i = numMemorySizeBytes - 1; i >= 0; i--) {
        *ptr = (memorySize & (0xFF << (8 * i))) >> (8 * i);
        ptr++;
    }

    client->send_size = UDS_0X35_REQ_BASE_LEN + numMemoryAddressBytes + numMemorySizeBytes;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param blockSequenceCounter
 * @param blockLength
 * @param fd
 * @return UDSClientError_t
 * @addtogroup transferData_0x36
 */
UDSClientError_t UDSSendTransferData(UDSClient_t *client, uint8_t blockSequenceCounter,
                                     const uint16_t blockLength, const uint8_t *data,
                                     uint16_t size) {
    PRE_REQUEST_CHECK();
    assert(blockLength > 2);         // blockLength must include SID and sequenceCounter
    assert(size + 2 <= blockLength); // data must fit inside blockLength - 2
    client->send_buf[0] = kSID_TRANSFER_DATA;
    client->send_buf[1] = blockSequenceCounter;
    memmove(&client->send_buf[UDS_0X36_REQ_BASE_LEN], data, size);
    UDS_DBG_PRINT("size: %d, blocklength: %d\n", size, blockLength);
    client->send_size = UDS_0X36_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

UDSClientError_t UDSSendTransferDataStream(UDSClient_t *client, uint8_t blockSequenceCounter,
                                           const uint16_t blockLength, FILE *fd) {
    PRE_REQUEST_CHECK();
    assert(blockLength > 2); // blockLength must include SID and sequenceCounter
    client->send_buf[0] = kSID_TRANSFER_DATA;
    client->send_buf[1] = blockSequenceCounter;

    uint16_t size = fread(&client->send_buf[2], 1, blockLength - 2, fd);
    if (size == 0) {
        return kUDS_CLIENT_ERR_REQ_NOT_SENT_EOF;
    }
    UDS_DBG_PRINT("size: %d, blocklength: %d\n", size, blockLength);
    client->send_size = UDS_0X36_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @return UDSClientError_t
 * @addtogroup requestTransferExit_0x37
 */
UDSClientError_t UDSSendRequestTransferExit(UDSClient_t *client) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_REQUEST_TRANSFER_EXIT;
    client->send_size = 1;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param dtcSettingType
 * @param data
 * @param size
 * @return UDSClientError_t
 * @addtogroup controlDTCSetting_0x85
 */
UDSClientError_t UDSCtrlDTCSetting(UDSClient_t *client, uint8_t dtcSettingType, uint8_t *data,
                                   uint16_t size) {
    PRE_REQUEST_CHECK();
    if (0x00 == dtcSettingType || 0x7F == dtcSettingType ||
        (0x03 <= dtcSettingType && dtcSettingType <= 0x3F)) {
        assert(0); // reserved vals
    }
    client->send_buf[0] = kSID_CONTROL_DTC_SETTING;
    client->send_buf[1] = dtcSettingType;

    if (NULL == data) {
        assert(size == 0);
    } else {
        assert(size > 0);
        if (size > client->send_buf_size - 2) {
            return kUDS_CLIENT_ERR_REQ_NOT_SENT_BUF_TOO_SMALL;
        }
        memmove(&client->send_buf[2], data, size);
    }
    client->send_size = 2 + size;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param level
 * @param data
 * @param size
 * @return UDSClientError_t
 * @addtogroup securityAccess_0x27
 */
UDSClientError_t UDSSendSecurityAccess(UDSClient_t *client, uint8_t level, uint8_t *data,
                                       uint16_t size) {
    PRE_REQUEST_CHECK();
    if (UDSSecurityAccessLevelIsReserved(level)) {
        return kUDS_CLIENT_ERR_REQ_NOT_SENT_INVALID_ARGS;
    }
    client->send_buf[0] = kSID_SECURITY_ACCESS;
    client->send_buf[1] = level;
    if (size) {
        assert(data);
        if (size > client->send_buf_size - UDS_0X27_REQ_BASE_LEN) {
            return kUDS_CLIENT_ERR_REQ_NOT_SENT_BUF_TOO_SMALL;
        }
    } else {
        assert(NULL == data);
    }

    memmove(&client->send_buf[UDS_0X27_REQ_BASE_LEN], data, size);
    client->send_size = UDS_0X27_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

// enum DLState {
//     DLStateInit,
//     DLStateRequestDL,
//     DLStateAwaitRequest,

// };

// typedef struct {
//     UDS_SEQUENCE_STRUCT_MEMBERS
//     const UDSClientDownloadParams_t params;
// } UDSClientDownloadSequence_t  ;

static UDSClientError_t requestDownload(UDSClient_t *client, UDSSequence_t *sequence) {
    UDSClientDownloadSequence_t *seq = (UDSClientDownloadSequence_t *)sequence;
    UDSSendRequestDownload(client, seq->dataFormatIdentifier, seq->addressAndLengthFormatIdentifier,
                           seq->memoryAddress, seq->memorySize);
    return kUDS_SEQ_ADVANCE;
}

static UDSClientError_t checkRequestDownloadResponse(UDSClient_t *client, UDSSequence_t *sequence) {
    UDSClientDownloadSequence_t *seq = (UDSClientDownloadSequence_t *)sequence;
    struct RequestDownloadResponse resp = {0};
    UDSClientError_t err = UDSUnpackRequestDownloadResponse(client, &resp);
    if (err) {
        return err;
    }
    seq->blockLength = resp.maxNumberOfBlockLength;
    if (0 == resp.maxNumberOfBlockLength) {
        return kUDS_CLIENT_ERR_RESP_SCHEMA_INVALID; // 响应格式对不上孚能规范
    }

    return kUDS_SEQ_ADVANCE;
}

static UDSClientError_t prepareToTransfer(UDSClient_t *client, UDSSequence_t *sequence) {
    UDSClientDownloadSequence_t *seq = (UDSClientDownloadSequence_t *)sequence;
    seq->blockSequenceCounter = 1;
    return kUDS_SEQ_ADVANCE;
}

static UDSClientError_t transferData(UDSClient_t *client, UDSSequence_t *sequence) {
    UDSClientDownloadSequence_t *seq = (UDSClientDownloadSequence_t *)sequence;
    if (kRequestStateIdle == client->state) {
        if (ferror(seq->fd)) {
            fclose(seq->fd);
            return kUDS_SEQ_ERR_FERROR; // 读取文件故障
        } else if (feof(seq->fd)) {     // 传完了
            return kUDS_SEQ_ADVANCE;
        } else {
            UDSSendTransferDataStream(client, seq->blockSequenceCounter++, seq->blockLength,
                                      seq->fd);
        }
    }
    return kUDS_SEQ_RUNNING;
}

static UDSClientError_t requestTransferExit(UDSClient_t *client, UDSSequence_t *sequence) {
    UDSSendRequestTransferExit(client);
    return kUDS_SEQ_ADVANCE;
}

static const UDSClientCallback downloadSequenceCallbacks[] = {
    requestDownload, UDSClientAwaitIdle,  checkRequestDownloadResponse, prepareToTransfer,
    transferData,    requestTransferExit, UDSClientAwaitIdle,           NULL};

UDSClientError_t UDSConfigDownload(UDSClientDownloadSequence_t *sequence,
                                   uint8_t dataFormatIdentifier,
                                   uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                                   size_t memorySize, FILE *fd) {
    assert(sequence);
    memset(sequence, 0, sizeof(*sequence));
    sequence->cbList = downloadSequenceCallbacks;
    sequence->cbIdx = 0;
    sequence->err = kUDS_SEQ_RUNNING;
    sequence->blockSequenceCounter = 1;
    sequence->dataFormatIdentifier = dataFormatIdentifier;
    sequence->addressAndLengthFormatIdentifier = addressAndLengthFormatIdentifier;
    sequence->memoryAddress = memoryAddress;
    sequence->memorySize = memorySize;
    sequence->fd = fd;
    return kUDS_CLIENT_OK;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return UDSClientError_t
 * @addtogroup securityAccess_0x27
 */
UDSClientError_t UDSUnpackSecurityAccessResponse(const UDSClient_t *client,
                                                 struct SecurityAccessResponse *resp) {
    assert(client);
    assert(resp);
    if (UDS_RESPONSE_SID_OF(kSID_SECURITY_ACCESS) != client->recv_buf[0]) {
        return kUDS_CLIENT_ERR_RESP_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X27_RESP_BASE_LEN) {
        return kUDS_CLIENT_ERR_RESP_TOO_SHORT;
    }
    resp->securityAccessType = client->recv_buf[1];
    resp->securitySeedLength = client->recv_size - UDS_0X27_RESP_BASE_LEN;
    resp->securitySeed = resp->securitySeedLength == 0 ? NULL : &client->recv_buf[2];
    return kUDS_CLIENT_OK;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return UDSClientError_t
 * @addtogroup routineControl_0x31
 */
UDSClientError_t UDSUnpackRoutineControlResponse(const UDSClient_t *client,
                                                 struct RoutineControlResponse *resp) {
    assert(client);
    assert(resp);
    if (UDS_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL) != client->recv_buf[0]) {
        return kUDS_CLIENT_ERR_RESP_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X31_RESP_MIN_LEN) {
        return kUDS_CLIENT_ERR_RESP_TOO_SHORT;
    }
    resp->routineControlType = client->recv_buf[1];
    resp->routineIdentifier = (client->recv_buf[2] << 8) + client->recv_buf[3];
    resp->routineStatusRecordLength = client->recv_size - UDS_0X31_RESP_MIN_LEN;
    resp->routineStatusRecord =
        resp->routineStatusRecordLength == 0 ? NULL : &client->recv_buf[UDS_0X31_RESP_MIN_LEN];
    return kUDS_CLIENT_OK;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return UDSClientError_t
 * @addtogroup requestDownload_0x34
 */
UDSClientError_t UDSUnpackRequestDownloadResponse(const UDSClient_t *client,
                                                  struct RequestDownloadResponse *resp) {
    assert(client);
    assert(resp);
    if (UDS_RESPONSE_SID_OF(kSID_REQUEST_DOWNLOAD) != client->recv_buf[0]) {
        return kUDS_CLIENT_ERR_RESP_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X34_RESP_BASE_LEN) {
        return kUDS_CLIENT_ERR_RESP_TOO_SHORT;
    }
    uint8_t maxNumberOfBlockLengthSize = (client->recv_buf[1] & 0xF0) >> 4;

    if (sizeof(resp->maxNumberOfBlockLength) < maxNumberOfBlockLengthSize) {
        UDS_DBG_PRINT("WARNING: sizeof(maxNumberOfBlockLength) > sizeof(size_t)");
        return kUDS_CLIENT_ERR_RESP_CANNOT_UNPACK;
    }
    resp->maxNumberOfBlockLength = 0;
    for (int byteIdx = 0; byteIdx < maxNumberOfBlockLengthSize; byteIdx++) {
        uint8_t byte = client->recv_buf[UDS_0X34_RESP_BASE_LEN + byteIdx];
        uint8_t shiftBytes = maxNumberOfBlockLengthSize - 1 - byteIdx;
        resp->maxNumberOfBlockLength |= byte << (8 * shiftBytes);
    }
    return kUDS_CLIENT_OK;
}

/**
 * @brief Check that the response is a valid UDS response
 *
 * @param ctx
 * @return UDSClientError_t
 */
static UDSClientError_t _ClientValidateResponse(const UDSClient_t *client) {

    if (client->recv_size < 1) {
        return kUDS_CLIENT_ERR_RESP_TOO_SHORT;
    }

    if (0x7F == client->recv_buf[0]) { // 否定响应
        if (client->recv_size < 2) {
            return kUDS_CLIENT_ERR_RESP_TOO_SHORT;
        } else if (client->send_buf[0] != client->recv_buf[1]) {
            return kUDS_CLIENT_ERR_RESP_SID_MISMATCH;
        } else if (kRequestCorrectlyReceived_ResponsePending == client->recv_buf[2]) {
            return kUDS_CLIENT_OK;
        } else if (client->_options_copy & NEG_RESP_IS_ERR) {
            return kUDS_CLIENT_ERR_RESP_NEGATIVE;
        } else {
            ;
        }
    } else { // 肯定响应
        if (UDS_RESPONSE_SID_OF(client->send_buf[0]) != client->recv_buf[0]) {
            return kUDS_CLIENT_ERR_RESP_SID_MISMATCH;
        }
    }

    return kUDS_CLIENT_OK;
}

/**
 * @brief Handle validated server response
 * @param client
 */
static inline void _ClientHandleResponse(UDSClient_t *client) {
    if (0x7F == client->recv_buf[0]) {
        if (kRequestCorrectlyReceived_ResponsePending == client->recv_buf[2]) {
            UDS_DBG_PRINT("got RCRRP, setting p2 timer\n");
            client->p2_timer = UDSMillis() + client->p2_star_ms;
            memset(client->recv_buf, 0, client->recv_buf_size);
            client->recv_size = 0;
            changeState(client, kRequestStateAwaitResponse);
            return;
        } else {
            ;
        }
    } else {
        uint8_t respSid = client->recv_buf[0];
        switch (UDS_REQUEST_SID_OF(respSid)) {
        case kSID_DIAGNOSTIC_SESSION_CONTROL: {
            if (client->recv_size < UDS_0X10_RESP_LEN) {
                UDS_DBG_PRINT("Error: SID %x response too short\n",
                              kSID_DIAGNOSTIC_SESSION_CONTROL);
                client->err = kUDS_CLIENT_ERR_RESP_TOO_SHORT;
                changeState(client, kRequestStateIdle);
                return;
            }

            if (client->_options_copy & IGNORE_SERVER_TIMINGS) {
                changeState(client, kRequestStateIdle);
                return;
            }

            uint16_t p2 = (client->recv_buf[2] << 8) + client->recv_buf[3];
            uint32_t p2_star = ((client->recv_buf[4] << 8) + client->recv_buf[5]) * 10;
            UDS_DBG_PRINT("received new timings: p2: %u, p2*: %u\n", p2, p2_star);
            client->p2_ms = p2;
            client->p2_star_ms = p2_star;
            break;
        }
        default:
            break;
        }
    }
    changeState(client, kRequestStateIdle);
}

void UDSClientPoll(UDSClient_t *client) {
    assert(client);
    UDSTpStatus_t tp_status = client->tp->poll(client->tp);
    switch (client->state) {
    case kRequestStateIdle: {
        client->options = client->defaultOptions;
        break;
    }
    case kRequestStateSending: {
        UDSTpAddr_t ta_type =
            client->_options_copy & FUNCTIONAL ? kTpAddrTypeFunctional : kTpAddrTypePhysical;
        ssize_t ret = 0;
        ret = client->tp->send(client->tp, client->send_buf, client->send_size, ta_type);

        if (ret < 0) {
            client->err = kUDS_CLIENT_ERR_REQ_NOT_SENT_TPORT_ERR;
            UDS_DBG_PRINT("tport err: %d\n", ret);
        } else if (0 == ret) {
            UDS_DBG_PRINT("send in progress...\n");
            ; // 等待发送成功
        } else if (client->send_size == ret) {
            changeState(client, kRequestStateAwaitSendComplete);
        } else {
            client->err = kUDS_CLIENT_ERR_REQ_NOT_SENT_BUF_TOO_SMALL;
        }
        break;
    }
    case kRequestStateAwaitSendComplete: {
        if (client->_options_copy & FUNCTIONAL) {
            // "The Functional addressing is applied only to single frame transmission"
            // Specification of Diagnostic Communication (Diagnostic on CAN - Network Layer)
            changeState(client, kRequestStateIdle);
        }
        if (tp_status & TP_SEND_INPROGRESS) {
            ; // await send complete
        } else {
            if (client->_options_copy & SUPPRESS_POS_RESP) {
                changeState(client, kRequestStateIdle);
            } else {
                changeState(client, kRequestStateAwaitResponse);
                client->p2_timer = UDSMillis() + client->p2_ms;
            }
        }
        break;
    }
    case kRequestStateAwaitResponse: {
        UDSTpAddr_t ta_type = kTpAddrTypePhysical;
        ssize_t ret =
            client->tp->recv(client->tp, client->recv_buf, client->recv_buf_size, &ta_type);

        if (kTpAddrTypeFunctional == ta_type) {
            break;
        }
        if (ret < 0) {
            client->err = kUDS_CLIENT_ERR_RESP_TPORT_ERR;
            changeState(client, kRequestStateIdle);
        } else if (0 == ret) {
            if (UDSTimeAfter(UDSMillis(), client->p2_timer)) {
                client->err = kUDS_CLIENT_ERR_REQ_TIMED_OUT;
                changeState(client, kRequestStateIdle);
            }
        } else {
            client->recv_size = ret;
            changeState(client, kRequestStateProcessResponse);
        }
        break;
    }
    case kRequestStateProcessResponse: {
        client->err = _ClientValidateResponse(client);
        if (kUDS_CLIENT_OK == client->err) {
            _ClientHandleResponse(client);
        } else {
            changeState(client, kRequestStateIdle);
        }
        break;
    }

    default:
        assert(0);
    }
}

UDSSequenceError_t UDSSequencePoll(UDSClient_t *client, UDSSequence_t *sequence) {
    assert(client);
    assert(sequence);
    assert(sequence->cbList);
    int err = kUDS_SEQ_RUNNING;

    UDSClientPoll(client);

    if (client->err) {
        err = kUDS_SEQ_ERR_CLIENT_ERR;
        goto done;
    }

    UDSClientCallback activeCallback = sequence->cbList[sequence->cbIdx];
    if (NULL == activeCallback) {
        err = kUDS_SEQ_COMPLETE;
        goto done;
    }

    err = activeCallback(client, sequence);

    if (err == kUDS_SEQ_ADVANCE) {
        sequence->cbIdx += 1;
        if (sequence->onChange) {
            sequence->onChange(sequence);
        }
        err = kUDS_SEQ_RUNNING;
    }

done:
    sequence->err = err;
    return sequence->err;
}

UDSClientError_t UDSClientAwaitIdle(UDSClient_t *client, UDSSequence_t *seq) {
    (void)seq;
    if (client->err) {
        return client->err;
    } else if (kRequestStateIdle == client->state) {
        return kUDS_SEQ_ADVANCE;
    } else {
        return kUDS_SEQ_RUNNING;
    }
}

UDSClientError_t UDSUnpackRDBIResponse(const UDSClient_t *client, uint16_t did, uint8_t *data,
                                       uint16_t size, uint16_t *offset) {
    assert(client);
    assert(data);
    assert(offset);
    if (0 == *offset) {
        *offset = UDS_0X22_RESP_BASE_LEN;
    }

    if (*offset + sizeof(did) > client->recv_size) {
        return kUDS_CLIENT_ERR_RESP_TOO_SHORT;
    }

    uint16_t theirDID = (client->recv_buf[*offset] << 8) + client->recv_buf[*offset + 1];
    if (theirDID != did) {
        return kUDS_CLIENT_ERR_RESP_DID_MISMATCH;
    }

    if (*offset + sizeof(uint16_t) + size > client->recv_size) {
        return kUDS_CLIENT_ERR_RESP_TOO_SHORT;
    }

    memmove(data, &client->recv_buf[*offset + sizeof(uint16_t)], size);

    *offset += sizeof(uint16_t) + size;
    return kUDS_CLIENT_OK;
}

void UDSSequenceInit(UDSSequence_t *sequence, const UDSClientCallback *cbList,
                     void (*onChange)(UDSSequence_t *sequence)) {
    assert(sequence);
    assert(cbList);
    sequence->cbList = cbList;
    sequence->onChange = onChange;
    sequence->cbIdx = 0;
    sequence->err = kUDS_SEQ_RUNNING;
}
