#include "iso14229.h"
#include <errno.h>
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
#define UDS_0X10_REQ_LEN 2U
#define UDS_0X10_RESP_LEN 6U
#define UDS_0X11_REQ_MIN_LEN 2U
#define UDS_0X11_RESP_BASE_LEN 2U
#define UDS_0X23_REQ_MIN_LEN 4U
#define UDS_0X23_RESP_BASE_LEN 1U
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
#define UDS_0X35_RESP_BASE_LEN 2U
#define UDS_0X36_REQ_BASE_LEN 2U
#define UDS_0X36_RESP_BASE_LEN 2U
#define UDS_0X37_REQ_BASE_LEN 1U
#define UDS_0X37_RESP_BASE_LEN 1U
#define UDS_0X3E_REQ_MIN_LEN 2U
#define UDS_0X3E_REQ_MAX_LEN 2U
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
    UDSTpIsoTpC_t *impl = (UDSTpIsoTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    isotp_poll(&impl->func_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
#elif UDS_TP == UDS_TP_ISOTP_SOCKET
#endif
    return status;
}
#endif

#if UDS_TP == UDS_TP_CUSTOM
#else
static ssize_t tp_recv(UDSTpHandle_t *hdl, void *buf, size_t count, UDSTpAddr_t *ta_type) {
    assert(hdl);
    assert(ta_type);
    assert(buf);
    int ret = -1;
#if UDS_TP == UDS_TP_ISOTP_C
    uint16_t size = 0;
    UDSTpIsoTpC_t *impl = (UDSTpIsoTpC_t *)hdl;
    struct {
        IsoTpLink *link;
        UDSTpAddr_t ta_type;
    } arr[] = {{&impl->phys_link, kTpAddrTypePhysical}, {&impl->func_link, kTpAddrTypeFunctional}};
    for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
        ret = isotp_receive(arr[i].link, buf, count, &size);
        switch (ret) {
        case ISOTP_RET_OK:
            *ta_type = arr[i].ta_type;
            ret = size;
            goto done;
        case ISOTP_RET_NO_DATA:
            ret = 0;
            continue;
        case ISOTP_RET_ERROR:
            ret = -1;
            goto done;
        default:
            ret = -2;
            goto done;
        }
    }
#elif UDS_TP == UDS_TP_ISOTP_SOCKET
    UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl;
    struct {
        int fd;
        UDSTpAddr_t ta_type;
    } arr[] = {{impl->phys_fd, kTpAddrTypePhysical}, {impl->func_fd, kTpAddrTypeFunctional}};
    for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
        ret = read(arr[i].fd, buf, count);
        if (ret < 0) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                ret = 0;
                continue;
            } else {
                UDS_DBG_PRINT("read failed: %d with errno: %d\n", ret, errno);
                if (EILSEQ == errno) {
                    UDS_DBG_PRINT("Perhaps I received multiple responses?\n");
                }
                goto done;
            }
        } else {
            *ta_type = arr[i].ta_type;
            goto done;
        }
    }
#endif
done:
    if (ret > 0) {
        UDS_DBG_PRINT("<<< ");
        UDS_DBG_PRINTHEX(buf, ret);
    }
    return ret;
}
#endif

#if UDS_TP == UDS_TP_CUSTOM
#else
static ssize_t tp_send(UDSTpHandle_t *hdl, const void *buf, size_t count, UDSTpAddr_t ta_type) {
    assert(hdl);
    ssize_t ret = -1;
#if UDS_TP == UDS_TP_ISOTP_C
    UDSTpIsoTpC_t *impl = (UDSTpIsoTpC_t *)hdl;
    IsoTpLink *link = NULL;
    switch (ta_type) {
    case kTpAddrTypePhysical:
        link = &impl->phys_link;
        break;
    case kTpAddrTypeFunctional:
        link = &impl->func_link;
        break;
    default:
        ret = -4;
        goto done;
    }

    int send_status = isotp_send(link, buf, count);
    switch (send_status) {
    case ISOTP_RET_OK:
        ret = count;
        goto done;
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        ret = send_status;
        goto done;
    }
#elif UDS_TP == UDS_TP_ISOTP_SOCKET
    UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl;
    int fd;
    switch (ta_type) {
    case kTpAddrTypePhysical:
        fd = impl->phys_fd;
        break;
    case kTpAddrTypeFunctional:
        fd = impl->func_fd;
        break;
    default:
        ret = -4;
        goto done;
    }
    ret = write(fd, buf, count);
    if (ret < 0) {
        perror("write");
    }
#endif
done:
    UDS_DBG_PRINT(">>> ");
    UDS_DBG_PRINTHEX(buf, ret);
    return ret;
}
#endif

#if UDS_TP == UDS_TP_ISOTP_SOCKET
static int LinuxSockBind(const char *if_name, uint32_t rxid, uint32_t txid) {
    int fd = 0;
    struct ifreq ifr = {0};
    struct sockaddr_can addr = {0};
    struct can_isotp_fc_options fcopts = {
        .bs = 0x10,
        .stmin = 3,
        .wftmax = 0,
    };

    if ((fd = socket(AF_CAN, SOCK_DGRAM | SOCK_NONBLOCK, CAN_ISOTP)) < 0) {
        fprintf(stderr, "socket: %s", strerror(errno));
        return -1;
    }

    if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_RECV_FC, &fcopts, sizeof(fcopts)) < 0) {
        perror("setsockopt");
        return -1;
    }

    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name) - 1);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "ioctl: %s %s\n", strerror(errno), if_name);
        return -1;
    }

    addr.can_family = AF_CAN;

    addr.can_addr.tp.rx_id = rxid;
    addr.can_addr.tp.tx_id = txid;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind: %s %s\n", strerror(errno), if_name);
        return -1;
    }
    printf("opened ISO-TP link fd: %d, rxid: %03x, txid: %03x\n", fd, rxid, txid);
    return fd;
}

static int LinuxSockTpOpen(UDSTpHandle_t *hdl, const char *if_name, uint32_t phys_rxid,
                           uint32_t phys_txid, uint32_t func_rxid, uint32_t func_txid) {
    assert(if_name);
    UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl;
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

static void LinuxSockTpClose(UDSTpHandle_t *hdl) {
    if (hdl) {
        UDSTpLinuxIsoTp_t *impl = (UDSTpLinuxIsoTp_t *)hdl;
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
#endif // #if UDS_TP == UDS_TP_ISOTP_SOCKET

ssize_t UDSTpSend(UDSTpHandle_t *tp, const uint8_t *msg, size_t size) {
    return tp->send(tp, &(UDSSDU_t){
                            .A_Mtype = UDS_A_MTYPE_DIAG,
                            .A_SA = 0x7E0,
                            .A_TA = 0x7E8,
                            .A_TA_Type = UDS_A_TA_TYPE_PHYSICAL,
                            .A_Length = size,
                            .A_Data = msg,
                        });
}

ssize_t UDSTpSendFunctional(UDSTpHandle_t *tp, const uint8_t *msg, size_t size) {
    return tp->send(tp, &(UDSSDU_t){
                            .A_Mtype = UDS_A_MTYPE_DIAG,
                            .A_SA = 0x7E0,
                            .A_TA = 0x7DF,
                            .A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL,
                            .A_Length = size,
                            .A_Data = msg,
                        });
}

void UDSSessInit(UDSSess_t *sess, const UDSSessConfig_t *cfg) {
    assert(sess);
    assert(cfg);
    memset(sess, 0, sizeof(*sess));
    sess->tp = cfg->tp;
    sess->source_addr = cfg->source_addr;
    sess->target_addr = cfg->target_addr;
    sess->source_addr_func = cfg->source_addr_func;
    sess->target_addr_func = cfg->target_addr_func;
}

UDSErr_t UDSSessSend(UDSSess_t *sess, const uint8_t *msg, size_t size) {
    assert(sess);
    assert(msg);
    assert(size);
    UDSTpSend(sess->tp, msg, size);
    return UDS_OK;
}

UDSErr_t UDSSessSendFunctional(UDSSess_t *sess, const uint8_t *msg, size_t size) {
    assert(sess);
    assert(msg);
    assert(size);
    UDSTpSendFunctional(sess->tp, msg, size);
    return UDS_OK;
}

void UDSSessPoll(UDSSess_t *sess) {
    assert(sess);
    UDSTpHandle_t *tp = sess->tp;
    UDSSDU_t msg = {
        .A_DataBufSize = sizeof(sess->recv_buf),
        .A_Data = sess->recv_buf,
    };
    ssize_t ret = tp->recv(tp, &msg);
    if (ret > 0) {
        sess->recv_size = ret;
    }
}

// ========================================================================
//                              Common
// ========================================================================

#if UDS_CUSTOM_MILLIS
#else
uint32_t UDSMillis() {
#if UDS_ARCH == UDS_ARCH_UNIX
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
#elif UDS_ARCH == UDS_ARCH_WINDOWS
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    long long milliseconds = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
    return milliseconds;
#else
#error "UDSMillis() undefined!"
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

typedef uint8_t (*UDSService)(UDSServer_t *self);

static inline uint8_t NegativeResponse(UDSServer_t *self, uint8_t response_code) {
    self->send_buf[0] = 0x7F;
    self->send_buf[1] = self->recv_buf[0];
    self->send_buf[2] = response_code;
    self->send_size = UDS_NEG_RESP_LEN;
    return response_code;
}

static inline void NoResponse(UDSServer_t *self) { self->send_size = 0; }

static uint8_t _0x10_DiagnosticSessionControl(UDSServer_t *self) {
    if (self->recv_size < UDS_0X10_REQ_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t sessType = self->recv_buf[1] & 0x4F;

    UDSDiagSessCtrlArgs_t args = {
        .type = sessType,
        .p2_ms = UDS_CLIENT_DEFAULT_P2_MS,
        .p2_star_ms = UDS_CLIENT_DEFAULT_P2_STAR_MS,
    };

    uint8_t err = self->fn(self, UDS_SRV_EVT_DiagSessCtrl, &args);

    if (kPositiveResponse != err) {
        return NegativeResponse(self, err);
    }

    self->sessionType = sessType;

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
    self->send_buf[2] = args.p2_ms >> 8;
    self->send_buf[3] = args.p2_ms;

    // resolution: 10ms
    self->send_buf[4] = (args.p2_star_ms / 10) >> 8;
    self->send_buf[5] = args.p2_star_ms / 10;

    self->send_size = UDS_0X10_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x11_ECUReset(UDSServer_t *self) {
    uint8_t resetType = self->recv_buf[1] & 0x3F;

    if (self->recv_size < UDS_0X11_REQ_MIN_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    UDSECUResetArgs_t args = {
        .type = resetType,
        .powerDownTimeMillis = UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS,
    };

    uint8_t err = self->fn(self, UDS_SRV_EVT_EcuReset, &args);

    if (kPositiveResponse == err) {
        self->notReadyToReceive = true;
        self->ecuResetScheduled = resetType;
        self->ecuResetTimer = UDSMillis() + args.powerDownTimeMillis;
    } else {
        return NegativeResponse(self, err);
    }

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ECU_RESET);
    self->send_buf[1] = resetType;

    if (kEnableRapidPowerShutDown == resetType) {
        uint32_t powerDownTime = args.powerDownTimeMillis / 1000;
        if (powerDownTime > 255) {
            powerDownTime = 255;
        }
        self->send_buf[2] = powerDownTime;
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

/**
 * @brief decode the addressAndLengthFormatIdentifier that appears in ReadMemoryByAddress (0x23),
 * DynamicallyDefineDataIdentifier (0x2C), RequestDownload (0X34)
 *
 * @param self
 * @param buf pointer to addressAndDataLengthFormatIdentifier in recv_buf
 * @param memoryAddress the decoded memory address
 * @param memorySize the decoded memory size
 * @return uint8_t
 */
static uint8_t decodeAddressAndLength(UDSServer_t *self, uint8_t *const buf, void **memoryAddress,
                                      size_t *memorySize) {
    assert(self);
    assert(memoryAddress);
    assert(memorySize);
    long long unsigned int tmp = 0;
    *memoryAddress = 0;
    *memorySize = 0;

    assert(buf >= self->recv_buf && buf <= self->recv_buf + sizeof(self->recv_buf));

    if (self->recv_size < 3) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t memorySizeLength = (buf[0] & 0xF0) >> 4;
    uint8_t memoryAddressLength = buf[0] & 0x0F;

    if (memorySizeLength == 0 || memorySizeLength > sizeof(size_t)) {
        return NegativeResponse(self, kRequestOutOfRange);
    }

    if (memoryAddressLength == 0 || memoryAddressLength > sizeof(size_t)) {
        return NegativeResponse(self, kRequestOutOfRange);
    }

    if (buf + memorySizeLength + memoryAddressLength > self->recv_buf + self->recv_size) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (int byteIdx = 0; byteIdx < memoryAddressLength; byteIdx++) {
        long long unsigned int byte = buf[1 + byteIdx];
        uint8_t shiftBytes = memoryAddressLength - 1 - byteIdx;
        tmp |= byte << (8 * shiftBytes);
    }
    *memoryAddress = (void *)tmp;

    for (int byteIdx = 0; byteIdx < memorySizeLength; byteIdx++) {
        uint8_t byte = buf[1 + memoryAddressLength + byteIdx];
        uint8_t shiftBytes = memorySizeLength - 1 - byteIdx;
        *memorySize |= (size_t)byte << (8 * shiftBytes);
    }
    return kPositiveResponse;
}

static uint8_t _0x23_ReadMemoryByAddress(UDSServer_t *self) {
    uint8_t ret = kPositiveResponse;
    void *address = 0;
    size_t length = 0;

    if (self->recv_size < UDS_0X23_REQ_MIN_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    ret = decodeAddressAndLength(self, &self->recv_buf[1], &address, &length);
    if (kPositiveResponse != ret) {
        return NegativeResponse(self, ret);
    }

    UDSReadMemByAddrArgs_t args = {
        .memAddr = address,
        .memSize = length,
        .copy = safe_copy,
    };

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_MEMORY_BY_ADDRESS);
    self->send_size = UDS_0X23_RESP_BASE_LEN;
    ret = self->fn(self, UDS_SRV_EVT_ReadMemByAddr, &args);
    if (kPositiveResponse != ret) {
        return NegativeResponse(self, ret);
    }
    if (self->send_size != UDS_0X23_RESP_BASE_LEN + length) {
        return kGeneralProgrammingFailure;
    }
    return kPositiveResponse;
}

static uint8_t _0x27_SecurityAccess(UDSServer_t *self) {
    uint8_t subFunction = self->recv_buf[1];
    uint8_t response = kPositiveResponse;

    if (UDSSecurityAccessLevelIsReserved(subFunction)) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
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
            .len = self->recv_size - UDS_0X27_REQ_BASE_LEN,
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
            UDSSecAccessRequestSeedArgs_t args = {
                .level = subFunction,
                .dataRecord = &self->recv_buf[UDS_0X27_REQ_BASE_LEN],
                .len = self->recv_size - UDS_0X27_REQ_BASE_LEN,
                .copySeed = safe_copy,
            };

            response = self->fn(self, UDS_SRV_EVT_SecAccessRequestSeed, &args);

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
        .len = self->recv_size - UDS_0X31_REQ_MIN_LEN,
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
    void *memoryAddress = 0;
    size_t memorySize = 0;

    if (self->xferIsActive) {
        return NegativeResponse(self, kConditionsNotCorrect);
    }

    if (self->recv_size < UDS_0X34_REQ_BASE_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    err = decodeAddressAndLength(self, &self->recv_buf[2], &memoryAddress, &memorySize);
    if (kPositiveResponse != err) {
        return NegativeResponse(self, err);
    }

    UDSRequestDownloadArgs_t args = {
        .addr = memoryAddress,
        .size = memorySize,
        .dataFormatIdentifier = self->recv_buf[1],
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
    self->xferBlockLength = args.maxNumberOfBlockLength;

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

static uint8_t _0x35_RequestUpload(UDSServer_t *self) {
    uint8_t err;
    void *memoryAddress = 0;
    size_t memorySize = 0;

    if (self->xferIsActive) {
        return NegativeResponse(self, kConditionsNotCorrect);
    }

    if (self->recv_size < UDS_0X35_REQ_BASE_LEN) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }

    err = decodeAddressAndLength(self, &self->recv_buf[2], &memoryAddress, &memorySize);
    if (kPositiveResponse != err) {
        return NegativeResponse(self, err);
    }

    UDSRequestUploadArgs_t args = {
        .addr = memoryAddress,
        .size = memorySize,
        .dataFormatIdentifier = self->recv_buf[1],
        .maxNumberOfBlockLength = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH,
    };

    err = self->fn(self, UDS_SRV_EVT_RequestUpload, &args);

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
    self->xferBlockLength = args.maxNumberOfBlockLength;

    uint8_t lengthFormatIdentifier = sizeof(args.maxNumberOfBlockLength) << 4;

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_UPLOAD);
    self->send_buf[1] = lengthFormatIdentifier;
    for (uint8_t idx = 0; idx < sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = sizeof(args.maxNumberOfBlockLength) - 1 - idx;
        uint8_t byte = args.maxNumberOfBlockLength >> (shiftBytes * 8);
        self->send_buf[UDS_0X35_RESP_BASE_LEN + idx] = byte;
    }
    self->send_size = UDS_0X35_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength);
    return kPositiveResponse;
}

static uint8_t _0x36_TransferData(UDSServer_t *self) {
    uint8_t err = kPositiveResponse;
    uint16_t request_data_len = self->recv_size - UDS_0X36_REQ_BASE_LEN;
    uint8_t blockSequenceCounter = 0;

    if (!self->xferIsActive) {
        return NegativeResponse(self, kUploadDownloadNotAccepted);
    }

    if (self->recv_size < UDS_0X36_REQ_BASE_LEN) {
        err = kIncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    blockSequenceCounter = self->recv_buf[1];

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

    {
        UDSTransferDataArgs_t args = {
            .data = &self->recv_buf[UDS_0X36_REQ_BASE_LEN],
            .len = self->recv_size - UDS_0X36_REQ_BASE_LEN,
            .maxRespLen = self->xferBlockLength - UDS_0X36_RESP_BASE_LEN,
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
            goto fail;
        }
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

    self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_TRANSFER_EXIT);
    self->send_size = UDS_0X37_RESP_BASE_LEN;

    UDSRequestTransferExitArgs_t args = {
        .data = &self->recv_buf[UDS_0X37_REQ_BASE_LEN],
        .len = self->recv_size - UDS_0X37_REQ_BASE_LEN,
        .copyResponse = safe_copy,
    };

    err = self->fn(self, UDS_SRV_EVT_RequestTransferExit, &args);

    switch (err) {
    case kPositiveResponse:
        ResetTransfer(self);
        return kPositiveResponse;
    case kRequestCorrectlyReceived_ResponsePending:
        return NegativeResponse(self, kRequestCorrectlyReceived_ResponsePending);
    default:
        ResetTransfer(self);
        return NegativeResponse(self, err);
    }
}

static uint8_t _0x3E_TesterPresent(UDSServer_t *self) {
    if ((self->recv_size < UDS_0X3E_REQ_MIN_LEN) ||
        (self->recv_size > UDS_0X3E_REQ_MAX_LEN)) {
        return NegativeResponse(self, kIncorrectMessageLengthOrInvalidFormat);
    }
    uint8_t zeroSubFunction = self->recv_buf[1];

    switch (zeroSubFunction) {
    case 0x00:
    case 0x80:
        self->s3_session_timeout_timer = UDSMillis() + self->s3_ms;
        self->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TESTER_PRESENT);
        self->send_buf[1] = 0x00;
        self->send_size = UDS_0X3E_RESP_LEN;
        return kPositiveResponse;
    default:
        return NegativeResponse(self, kSubFunctionNotSupported);
    }
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
        return _0x23_ReadMemoryByAddress;
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
        return _0x35_RequestUpload;
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
    assert(service);
    assert(self->fn); // service handler functions will call self->fn. it must be valid

    switch (sid) {
    /* CASE Service_with_sub-function */
    /* test if service with sub-function is supported */
    case kSID_DIAGNOSTIC_SESSION_CONTROL:
    case kSID_ECU_RESET:
    case kSID_SECURITY_ACCESS:
    case kSID_COMMUNICATION_CONTROL:
    case kSID_ROUTINE_CONTROL:
    case kSID_TESTER_PRESENT:
    case kSID_CONTROL_DTC_SETTING: {
        response = service(self);

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
    case kSID_WRITE_DATA_BY_IDENTIFIER:
    case kSID_REQUEST_DOWNLOAD:
    case kSID_REQUEST_UPLOAD:
    case kSID_TRANSFER_DATA:
    case kSID_REQUEST_TRANSFER_EXIT: {
        response = service(self);
        break;
    }

    /* CASE Service_not_implemented */
    /* shouldn't get this far as getServiceForSID(sid) will return NULL*/
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
    case kSID_READ_DTC_INFORMATION:
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
    case kSID_INPUT_CONTROL_BY_IDENTIFIER:
    case kSID_REQUEST_FILE_TRANSFER:
    case kSID_WRITE_MEMORY_BY_ADDRESS:
    case kSID_ACCESS_TIMING_PARAMETER:
    case kSID_SECURED_DATA_TRANSMISSION:
    case kSID_RESPONSE_ON_EVENT:
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
        UDSSDU_t msg = {
            .A_Mtype = UDS_A_MTYPE_DIAG,
            .A_SA = self->source_addr,
            .A_TA = self->target_addr,
            .A_TA_Type = UDS_A_TA_TYPE_PHYSICAL, // server responses are always physical
            .A_Length = self->send_size,
            .A_Data = self->send_buf,
        };
        int result = self->tp->send(self->tp, &msg);
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
    assert(cfg->source_addr != cfg->target_addr);
    assert(cfg->target_addr != cfg->source_addr_func);
    assert(cfg->source_addr_func != cfg->source_addr);
    memset(self, 0, sizeof(UDSServer_t));
    self->recv_buf_size = sizeof(self->recv_buf);
    self->send_buf_size = sizeof(self->send_buf);
    self->p2_ms = UDS_SERVER_DEFAULT_P2_MS;
    self->p2_star_ms = UDS_SERVER_DEFAULT_P2_STAR_MS;
    self->s3_ms = UDS_SERVER_DEFAULT_S3_MS;
    self->fn = cfg->fn;
    self->sessionType = kDefaultSession;
    self->source_addr = cfg->source_addr;
    self->target_addr = cfg->target_addr;
    self->source_addr_func = cfg->source_addr_func;

    // Initialize p2_timer to an already past time, otherwise the server's
    // response to incoming messages will be delayed.
    self->p2_timer = UDSMillis() - self->p2_ms;

    // Set the session timeout for s3 milliseconds from now.
    self->s3_session_timeout_timer = UDSMillis() + self->s3_ms;

#if UDS_TP == UDS_TP_CUSTOM
    assert(cfg->tp);
    assert(cfg->tp->recv);
    assert(cfg->tp->send);
    assert(cfg->tp->poll);
    self->tp = cfg->tp;
#elif UDS_TP == UDS_TP_ISOTP_C
    assert(cfg->target_addr != cfg->source_addr_func && cfg->source_addr_func != cfg->source_addr);
    UDSTpIsoTpC_t *tp = &self->tp_impl;
    isotp_init_link(&tp->phys_link, cfg->target_addr, self->send_buf, self->send_buf_size,
                    self->recv_buf, self->recv_buf_size);
    isotp_init_link(&tp->func_link, cfg->target_addr, tp->func_send_buf, sizeof(tp->func_send_buf),
                    tp->func_recv_buf, sizeof(tp->func_recv_buf));
    self->tp = (UDSTpHandle_t *)tp;
    self->tp->poll = tp_poll;
    self->tp->send = tp_send;
    self->tp->recv = tp_recv;
#elif UDS_TP == UDS_TP_ISOTP_SOCKET
    self->tp = (UDSTpHandle_t *)&self->tp_impl;
    if (LinuxSockTpOpen(self->tp, cfg->if_name, cfg->source_addr, cfg->target_addr,
                        cfg->source_addr_func, cfg->target_addr)) {
        return UDS_ERR;
    }
#endif
    return UDS_OK;
}

void UDSServerDeInit(UDSServer_t *self) {
#if UDS_TP == UDS_TP_ISOTP_SOCKET
    LinuxSockTpClose(self->tp);
#endif
}

void UDSServerPoll(UDSServer_t *self) {
    // UDS-1-2013 Figure 38: Session Timeout (S3)
    if (kDefaultSession != self->sessionType &&
        UDSTimeAfter(UDSMillis(), self->s3_session_timeout_timer)) {
        if (self->fn) {
            self->fn(self, UDS_SRV_EVT_SessionTimeout, NULL);
        }
    }

    if (self->ecuResetScheduled && UDSTimeAfter(UDSMillis(), self->ecuResetTimer)) {
        if (self->fn) {
            self->fn(self, UDS_SRV_EVT_DoScheduledReset, &self->ecuResetScheduled);
        }
    }

    UDSTpStatus_t tp_status = self->tp->poll(self->tp);
    if (tp_status & UDS_TP_SEND_IN_PROGRESS) {
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
    if (UDSTimeAfter(UDSMillis(), self->p2_timer)) {
        UDSSDU_t msg = {
            .A_DataBufSize = self->recv_buf_size,
            .A_Data = self->recv_buf,
        };
        size = self->tp->recv(self->tp, &msg);
        if (size > 0) {
            if (msg.A_TA == self->source_addr) {
                msg.A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
            } else if (msg.A_TA == self->source_addr_func) {
                msg.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
            } else {
                UDS_DBG_PRINT("received message from unknown source address %x\n", msg.A_TA);
                return;
            }
            self->recv_size = size;
            ProcessLink(self, msg.A_TA_Type);
            self->p2_timer = UDSMillis() + self->p2_ms;
        } else if (size == 0) {
            ;
        } else {
            UDS_DBG_PRINT("tp_recv failed with err %d on tp %d\n", size, msg.A_TA_Type);
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
    client->err = UDS_OK;
}

UDSErr_t UDSClientInit(UDSClient_t *client, const UDSClientConfig_t *cfg) {
    assert(client);
    assert(cfg);
    assert(cfg->target_addr != cfg->source_addr);
    assert(cfg->source_addr != cfg->target_addr_func);
    assert(cfg->target_addr_func != cfg->target_addr);
    memset(client, 0, sizeof(*client));

    client->p2_ms = UDS_CLIENT_DEFAULT_P2_MS;
    client->p2_star_ms = UDS_CLIENT_DEFAULT_P2_STAR_MS;
    client->recv_buf_size = sizeof(client->recv_buf);
    client->send_buf_size = sizeof(client->send_buf);
    client->source_addr = cfg->source_addr;
    client->target_addr = cfg->target_addr;
    client->target_addr_func = cfg->target_addr_func;

    if (client->p2_star_ms < client->p2_ms) {
        fprintf(stderr, "p2_star_ms must be >= p2_ms\n");
        client->p2_star_ms = client->p2_ms;
    }

#if UDS_TP == UDS_TP_CUSTOM
    assert(cfg->tp);
    assert(cfg->tp->recv);
    assert(cfg->tp->send);
    assert(cfg->tp->poll);
    client->tp = cfg->tp;
#elif UDS_TP == UDS_TP_ISOTP_C
    assert(cfg->source_addr != cfg->target_addr_func && cfg->target_addr_func != cfg->target_addr);
    UDSTpIsoTpC_t *tp = (UDSTpIsoTpC_t *)&client->tp_impl;
    isotp_init_link(&tp->phys_link, cfg->target_addr, client->send_buf, client->send_buf_size,
                    client->recv_buf, client->recv_buf_size);
    isotp_init_link(&tp->func_link, cfg->target_addr_func, tp->func_send_buf,
                    sizeof(tp->func_send_buf), tp->func_recv_buf, sizeof(tp->func_recv_buf));
    client->tp = (UDSTpHandle_t *)tp;
    client->tp->poll = tp_poll;
    client->tp->send = tp_send;
    client->tp->recv = tp_recv;
#elif UDS_TP == UDS_TP_ISOTP_SOCKET
    client->tp = (UDSTpHandle_t *)&client->tp_impl;
    if (LinuxSockTpOpen(client->tp, cfg->if_name, cfg->source_addr, cfg->target_addr,
                        cfg->source_addr, cfg->target_addr_func)) {
        return UDS_ERR;
    }
    assert(client->tp);
#endif

    clearRequestContext(client);
    return UDS_OK;
}

void UDSClientDeInit(UDSClient_t *client) {
#if UDS_TP == UDS_TP_ISOTP_SOCKET
    LinuxSockTpClose(client->tp);
#endif
}

static void changeState(UDSClient_t *client, enum UDSClientRequestState state) {
    // printf("client state: %d -> %d\n", client->state, state);
    client->state = state;
}

/**
 * @brief Check that the response is a valid UDS response
 *
 * @param ctx
 * @return UDSErr_t
 */
static UDSErr_t _ClientValidateResponse(const UDSClient_t *client) {

    if (client->recv_size < 1) {
        return UDS_ERR_RESP_TOO_SHORT;
    }

    if (0x7F == client->recv_buf[0]) { // 否定响应
        if (client->recv_size < 2) {
            return UDS_ERR_RESP_TOO_SHORT;
        } else if (client->send_buf[0] != client->recv_buf[1]) {
            return UDS_ERR_SID_MISMATCH;
        } else if (kRequestCorrectlyReceived_ResponsePending == client->recv_buf[2]) {
            return UDS_OK;
        } else if (client->_options_copy & UDS_NEG_RESP_IS_ERR) {
            return UDS_ERR_NEG_RESP;
        } else {
            ;
        }
    } else { // 肯定响应
        if (UDS_RESPONSE_SID_OF(client->send_buf[0]) != client->recv_buf[0]) {
            return UDS_ERR_SID_MISMATCH;
        }
        switch (client->send_buf[0]) {
        case kSID_ECU_RESET:
            if (client->recv_size < 2) {
                return UDS_ERR_RESP_TOO_SHORT;
            } else if (client->send_buf[1] != client->recv_buf[1]) {
                return UDS_ERR_SUBFUNCTION_MISMATCH;
            } else {
                ;
            }
            break;
        }
    }

    return UDS_OK;
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
                client->err = UDS_ERR_RESP_TOO_SHORT;
                changeState(client, kRequestStateIdle);
                return;
            }

            if (client->_options_copy & UDS_IGNORE_SRV_TIMINGS) {
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

/**
 * @brief execute the client request state machine
 * @param client
 */
static void PollLowLevel(UDSClient_t *client) {
    assert(client);
    UDSTpStatus_t tp_status = client->tp->poll(client->tp);
    switch (client->state) {
    case kRequestStateIdle: {
        client->options = client->defaultOptions;
        break;
    }
    case kRequestStateSending: {
        UDSTpAddr_t ta_type =
            client->_options_copy & UDS_FUNCTIONAL ? kTpAddrTypeFunctional : kTpAddrTypePhysical;
        ssize_t ret = 0;
        UDSSDU_t msg = {
            .A_Mtype = UDS_A_MTYPE_DIAG,
            .A_SA = client->source_addr,
            .A_TA = ta_type == kTpAddrTypePhysical ? client->target_addr : client->target_addr_func,
            .A_TA_Type = ta_type,
            .A_Length = client->send_size,
            .A_Data = client->send_buf,
        };
        ret = client->tp->send(client->tp, &msg);

        if (ret < 0) {
            client->err = UDS_ERR_TPORT;
            UDS_DBG_PRINT("tport err: %ld\n", ret);
        } else if (0 == ret) {
            UDS_DBG_PRINT("send in progress...\n");
            ; // 等待发送成功
        } else if (client->send_size == ret) {
            changeState(client, kRequestStateAwaitSendComplete);
        } else {
            client->err = UDS_ERR_BUFSIZ;
        }
        break;
    }
    case kRequestStateAwaitSendComplete: {
        if (client->_options_copy & UDS_FUNCTIONAL) {
            // "The Functional addressing is applied only to single frame transmission"
            // Specification of Diagnostic Communication (Diagnostic on CAN - Network Layer)
            changeState(client, kRequestStateIdle);
        }
        if (tp_status & UDS_TP_SEND_IN_PROGRESS) {
            ; // await send complete
        } else {
            if (client->_options_copy & UDS_SUPPRESS_POS_RESP) {
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
        UDSSDU_t msg = {
            .A_DataBufSize = client->recv_buf_size,
            .A_Data = client->recv_buf,
        };
        ssize_t ret = client->tp->recv(client->tp, &msg);

        if (kTpAddrTypeFunctional == ta_type) {
            break;
        }
        if (ret < 0) {
            client->err = UDS_ERR_TPORT;
            changeState(client, kRequestStateIdle);
        } else if (0 == ret) {
            if (UDSTimeAfter(UDSMillis(), client->p2_timer)) {
                client->err = UDS_ERR_TIMEOUT;
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
        if (UDS_OK == client->err) {
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

static UDSErr_t _SendRequest(UDSClient_t *client) {
    client->_options_copy = client->options;

    if (client->_options_copy & UDS_SUPPRESS_POS_RESP) {
        // UDS-1:2013 8.2.2 Table 11
        client->send_buf[1] |= 0x80;
    }

    changeState(client, kRequestStateSending);
    PollLowLevel(client); // poll once to begin sending immediately
    return UDS_OK;
}

#define PRE_REQUEST_CHECK()                                                                        \
    if (kRequestStateIdle != client->state) {                                                      \
        return UDS_ERR_BUSY;                                                                       \
    }                                                                                              \
    clearRequestContext(client);

UDSErr_t UDSSendBytes(UDSClient_t *client, const uint8_t *data, uint16_t size) {
    PRE_REQUEST_CHECK();
    if (size > client->send_buf_size) {
        return UDS_ERR_BUFSIZ;
    }
    memmove(client->send_buf, data, size);
    client->send_size = size;
    return _SendRequest(client);
}

UDSErr_t UDSSendECUReset(UDSClient_t *client, UDSECUReset_t type) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_ECU_RESET;
    client->send_buf[1] = type;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSErr_t UDSSendDiagSessCtrl(UDSClient_t *client, enum UDSDiagnosticSessionType mode) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_DIAGNOSTIC_SESSION_CONTROL;
    client->send_buf[1] = mode;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSErr_t UDSSendCommCtrl(UDSClient_t *client, enum UDSCommunicationControlType ctrl,
                         enum UDSCommunicationType comm) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_COMMUNICATION_CONTROL;
    client->send_buf[1] = ctrl;
    client->send_buf[2] = comm;
    client->send_size = 3;
    return _SendRequest(client);
}

UDSErr_t UDSSendTesterPresent(UDSClient_t *client) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_TESTER_PRESENT;
    client->send_buf[1] = 0;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSErr_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                     const uint16_t numDataIdentifiers) {
    PRE_REQUEST_CHECK();
    assert(didList);
    assert(numDataIdentifiers);
    client->send_buf[0] = kSID_READ_DATA_BY_IDENTIFIER;
    for (int i = 0; i < numDataIdentifiers; i++) {
        uint16_t offset = 1 + sizeof(uint16_t) * i;
        if (offset + 2 > client->send_buf_size) {
            return UDS_ERR_INVALID_ARG;
        }
        (client->send_buf + offset)[0] = (didList[i] & 0xFF00) >> 8;
        (client->send_buf + offset)[1] = (didList[i] & 0xFF);
    }
    client->send_size = 1 + (numDataIdentifiers * sizeof(uint16_t));
    return _SendRequest(client);
}

UDSErr_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                     uint16_t size) {
    PRE_REQUEST_CHECK();
    assert(data);
    assert(size);
    client->send_buf[0] = kSID_WRITE_DATA_BY_IDENTIFIER;
    if (client->send_buf_size <= 3 || size > client->send_buf_size - 3) {
        return UDS_ERR_BUFSIZ;
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
 * @return UDSErr_t
 * @addtogroup routineControl_0x31
 */
UDSErr_t UDSSendRoutineCtrl(UDSClient_t *client, enum RoutineControlType type,
                            uint16_t routineIdentifier, const uint8_t *data, uint16_t size) {
    PRE_REQUEST_CHECK();
    client->send_buf[0] = kSID_ROUTINE_CONTROL;
    client->send_buf[1] = type;
    client->send_buf[2] = routineIdentifier >> 8;
    client->send_buf[3] = routineIdentifier;
    if (size) {
        assert(data);
        if (size > client->send_buf_size - UDS_0X31_REQ_MIN_LEN) {
            return UDS_ERR_BUFSIZ;
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
 * @return UDSErr_t
 * @addtogroup requestDownload_0x34
 */
UDSErr_t UDSSendRequestDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                                size_t memorySize) {
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
 * @return UDSErr_t
 * @addtogroup requestDownload_0x35
 */
UDSErr_t UDSSendRequestUpload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                              uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                              size_t memorySize) {
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
 * @return UDSErr_t
 * @addtogroup transferData_0x36
 */
UDSErr_t UDSSendTransferData(UDSClient_t *client, uint8_t blockSequenceCounter,
                             const uint16_t blockLength, const uint8_t *data, uint16_t size) {
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

UDSErr_t UDSSendTransferDataStream(UDSClient_t *client, uint8_t blockSequenceCounter,
                                   const uint16_t blockLength, FILE *fd) {
    PRE_REQUEST_CHECK();
    assert(blockLength > 2); // blockLength must include SID and sequenceCounter
    client->send_buf[0] = kSID_TRANSFER_DATA;
    client->send_buf[1] = blockSequenceCounter;

    uint16_t size = fread(&client->send_buf[2], 1, blockLength - 2, fd);
    UDS_DBG_PRINT("size: %d, blocklength: %d\n", size, blockLength);
    client->send_size = UDS_0X36_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @return UDSErr_t
 * @addtogroup requestTransferExit_0x37
 */
UDSErr_t UDSSendRequestTransferExit(UDSClient_t *client) {
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
 * @return UDSErr_t
 * @addtogroup controlDTCSetting_0x85
 */
UDSErr_t UDSCtrlDTCSetting(UDSClient_t *client, uint8_t dtcSettingType, uint8_t *data,
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
            return UDS_ERR_BUFSIZ;
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
 * @return UDSErr_t
 * @addtogroup securityAccess_0x27
 */
UDSErr_t UDSSendSecurityAccess(UDSClient_t *client, uint8_t level, uint8_t *data, uint16_t size) {
    PRE_REQUEST_CHECK();
    if (UDSSecurityAccessLevelIsReserved(level)) {
        return UDS_ERR_INVALID_ARG;
    }
    client->send_buf[0] = kSID_SECURITY_ACCESS;
    client->send_buf[1] = level;
    if (size) {
        assert(data);
        if (size > client->send_buf_size - UDS_0X27_REQ_BASE_LEN) {
            return UDS_ERR_BUFSIZ;
        }
    } else {
        assert(NULL == data);
    }

    memmove(&client->send_buf[UDS_0X27_REQ_BASE_LEN], data, size);
    client->send_size = UDS_0X27_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

typedef struct {
    uint8_t dataFormatIdentifier;
    uint8_t addressAndLengthFormatIdentifier;
    size_t memoryAddress;
    size_t memorySize;
    FILE *fd;
    uint8_t blockSequenceCounter;
    uint16_t blockLength;
} UDSClientDownloadSequence_t;

static UDSSeqState_t requestDownload(UDSClient_t *client) {
    UDSClientDownloadSequence_t *pL_Seq = (UDSClientDownloadSequence_t *)client->cbData;
    UDSSendRequestDownload(client, pL_Seq->dataFormatIdentifier,
                           pL_Seq->addressAndLengthFormatIdentifier, pL_Seq->memoryAddress,
                           pL_Seq->memorySize);
    return UDSSeqStateGotoNext;
}

static UDSSeqState_t checkRequestDownloadResponse(UDSClient_t *client) {
    UDSClientDownloadSequence_t *pL_Seq = (UDSClientDownloadSequence_t *)client->cbData;
    struct RequestDownloadResponse resp = {0};
    UDSErr_t err = UDSUnpackRequestDownloadResponse(client, &resp);
    if (err) {
        client->err = err;
        return UDSSeqStateDone;
    }
    pL_Seq->blockLength = resp.maxNumberOfBlockLength;
    if (0 == resp.maxNumberOfBlockLength) {
        client->err = UDS_ERR;
        return UDSSeqStateDone;
    }
    return UDSSeqStateGotoNext;
}

static UDSSeqState_t prepareToTransfer(UDSClient_t *client) {
    UDSClientDownloadSequence_t *pL_Seq = (UDSClientDownloadSequence_t *)client->cbData;
    pL_Seq->blockSequenceCounter = 1;
    return UDSSeqStateGotoNext;
}

static UDSSeqState_t transferData(UDSClient_t *client) {
    UDSClientDownloadSequence_t *pL_Seq = (UDSClientDownloadSequence_t *)client->cbData;
    if (kRequestStateIdle == client->state) {
        if (ferror(pL_Seq->fd)) {
            fclose(pL_Seq->fd);
            client->err = UDS_ERR_FILE_IO; // 读取文件故障
            return UDSSeqStateDone;
        } else if (feof(pL_Seq->fd)) { // 传完了
            return UDSSeqStateGotoNext;
        } else {
            UDSSendTransferDataStream(client, pL_Seq->blockSequenceCounter++, pL_Seq->blockLength,
                                      pL_Seq->fd);
        }
    }
    return UDSSeqStateRunning;
}

static UDSSeqState_t requestTransferExit(UDSClient_t *client) {
    UDSSendRequestTransferExit(client);
    return UDSSeqStateGotoNext;
}

UDSErr_t UDSConfigDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                           uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                           size_t memorySize, FILE *fd) {

    static const UDSClientCallback callbacks[] = {
        requestDownload, UDSClientAwaitIdle,  checkRequestDownloadResponse, prepareToTransfer,
        transferData,    requestTransferExit, UDSClientAwaitIdle,           NULL};
    static UDSClientDownloadSequence_t seq = {0};
    memset(&seq, 0, sizeof(seq));
    seq.blockSequenceCounter = 1;
    seq.dataFormatIdentifier = dataFormatIdentifier;
    seq.addressAndLengthFormatIdentifier = addressAndLengthFormatIdentifier;
    seq.memoryAddress = memoryAddress;
    seq.memorySize = memorySize;
    seq.fd = fd;
    client->cbList = callbacks;
    client->cbIdx = 0;
    client->cbData = &seq;
    return UDS_OK;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return UDSErr_t
 * @addtogroup securityAccess_0x27
 */
UDSErr_t UDSUnpackSecurityAccessResponse(const UDSClient_t *client,
                                         struct SecurityAccessResponse *resp) {
    assert(client);
    assert(resp);
    if (UDS_RESPONSE_SID_OF(kSID_SECURITY_ACCESS) != client->recv_buf[0]) {
        return UDS_ERR_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X27_RESP_BASE_LEN) {
        return UDS_ERR_RESP_TOO_SHORT;
    }
    resp->securityAccessType = client->recv_buf[1];
    resp->securitySeedLength = client->recv_size - UDS_0X27_RESP_BASE_LEN;
    resp->securitySeed = resp->securitySeedLength == 0 ? NULL : &client->recv_buf[2];
    return UDS_OK;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return UDSErr_t
 * @addtogroup routineControl_0x31
 */
UDSErr_t UDSUnpackRoutineControlResponse(const UDSClient_t *client,
                                         struct RoutineControlResponse *resp) {
    assert(client);
    assert(resp);
    if (UDS_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL) != client->recv_buf[0]) {
        return UDS_ERR_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X31_RESP_MIN_LEN) {
        return UDS_ERR_RESP_TOO_SHORT;
    }
    resp->routineControlType = client->recv_buf[1];
    resp->routineIdentifier = (client->recv_buf[2] << 8) + client->recv_buf[3];
    resp->routineStatusRecordLength = client->recv_size - UDS_0X31_RESP_MIN_LEN;
    resp->routineStatusRecord =
        resp->routineStatusRecordLength == 0 ? NULL : &client->recv_buf[UDS_0X31_RESP_MIN_LEN];
    return UDS_OK;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return UDSErr_t
 * @addtogroup requestDownload_0x34
 */
UDSErr_t UDSUnpackRequestDownloadResponse(const UDSClient_t *client,
                                          struct RequestDownloadResponse *resp) {
    assert(client);
    assert(resp);
    if (UDS_RESPONSE_SID_OF(kSID_REQUEST_DOWNLOAD) != client->recv_buf[0]) {
        return UDS_ERR_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X34_RESP_BASE_LEN) {
        return UDS_ERR_RESP_TOO_SHORT;
    }
    uint8_t maxNumberOfBlockLengthSize = (client->recv_buf[1] & 0xF0) >> 4;

    if (sizeof(resp->maxNumberOfBlockLength) < maxNumberOfBlockLengthSize) {
        UDS_DBG_PRINT("WARNING: sizeof(maxNumberOfBlockLength) > sizeof(size_t)");
        return UDS_ERR;
    }
    resp->maxNumberOfBlockLength = 0;
    for (int byteIdx = 0; byteIdx < maxNumberOfBlockLengthSize; byteIdx++) {
        uint8_t byte = client->recv_buf[UDS_0X34_RESP_BASE_LEN + byteIdx];
        uint8_t shiftBytes = maxNumberOfBlockLengthSize - 1 - byteIdx;
        resp->maxNumberOfBlockLength |= byte << (8 * shiftBytes);
    }
    return UDS_OK;
}

bool UDSClientPoll(UDSClient_t *client) {
    PollLowLevel(client);

    if (client->err) {
        return UDS_CLIENT_IDLE;
    }

    if (kRequestStateIdle != client->state) {
        return UDS_CLIENT_RUNNING;
    }

    if (NULL == client->cbList) {
        return UDS_CLIENT_IDLE;
    }

    UDSClientCallback activeCallback = client->cbList[client->cbIdx];

    if (NULL == activeCallback) {
        return UDS_CLIENT_IDLE;
    }

    UDSSeqState_t state = activeCallback(client);

    switch (state) {
    case UDSSeqStateDone:
        return UDS_CLIENT_IDLE;
    case UDSSeqStateRunning:
        return UDS_CLIENT_RUNNING;
    case UDSSeqStateGotoNext: {
        client->cbIdx += 1;
        return UDS_CLIENT_RUNNING;
    }
    default:
        assert(0);
        return UDS_CLIENT_IDLE;
    }
}

void UDSClientPoll2(UDSClient_t *client,
                    int (*fn)(UDSClient_t *client, UDSEvent_t evt, void *ev_data, void *fn_data),
                    void *fn_data) {
    UDSClientPoll(client);
}

UDSSeqState_t UDSClientAwaitIdle(UDSClient_t *client) {
    if (client->err) {
        return UDSSeqStateDone;
    } else if (kRequestStateIdle == client->state) {
        return UDSSeqStateGotoNext;
    } else {
        return UDSSeqStateRunning;
    }
}

UDSErr_t UDSUnpackRDBIResponse(const UDSClient_t *client, uint16_t did, uint8_t *data,
                               uint16_t size, uint16_t *offset) {
    assert(client);
    assert(data);
    assert(offset);
    if (0 == *offset) {
        *offset = UDS_0X22_RESP_BASE_LEN;
    }

    if (*offset + sizeof(did) > client->recv_size) {
        return UDS_ERR_RESP_TOO_SHORT;
    }

    uint16_t theirDID = (client->recv_buf[*offset] << 8) + client->recv_buf[*offset + 1];
    if (theirDID != did) {
        return UDS_ERR_DID_MISMATCH;
    }

    if (*offset + sizeof(uint16_t) + size > client->recv_size) {
        return UDS_ERR_RESP_TOO_SHORT;
    }

    memmove(data, &client->recv_buf[*offset + sizeof(uint16_t)], size);

    *offset += sizeof(uint16_t) + size;
    return UDS_OK;
}
