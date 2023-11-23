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

/**
 * @brief 
 * 
 * @param hdl 
 * @param info, if NULL, the default values are used:
 *   A_Mtype: message type (diagnostic (DEFAULT), remote diagnostic, secure diagnostic, secure
    * remote diagnostic)
    * A_TA_Type: application target address type (physical (DEFAULT) or functional)
    * A_SA: unused
    * A_TA: unused
    * A_AE: unused
 * @return ssize_t 
 */
ssize_t UDSTpGetSendBuf(struct UDSTpHandle *hdl, uint8_t **buf) {
    assert(hdl);
    assert(hdl->get_send_buf);
    return hdl->get_send_buf(hdl, buf);
}
ssize_t UDSTpSend(struct UDSTpHandle *hdl, const uint8_t *buf, ssize_t len, UDSSDU_t *info) {
    assert(hdl);
    assert(hdl->send);
    return hdl->send(hdl, (uint8_t*)buf, len, info);
}

UDSTpStatus_t UDSTpPoll(struct UDSTpHandle *hdl) {
    assert(hdl);
    assert(hdl->poll);
    return hdl->poll(hdl);
}

ssize_t UDSTpPeek(struct UDSTpHandle *hdl, uint8_t **buf, UDSSDU_t *info) {
    assert(hdl);
    assert(hdl->peek);
    return hdl->peek(hdl, buf, info);
}

const uint8_t *UDSTpGetRecvBuf(struct UDSTpHandle *hdl, size_t *p_len) {
    assert(hdl);
    ssize_t len = 0;
    uint8_t *buf = NULL;
    len = UDSTpPeek(hdl, &buf, NULL);
    if (len > 0) {
        if (p_len) {
            *p_len = len;
        }
        return buf;
    } else {
        return NULL;
    }
}

size_t UDSTpGetRecvLen(UDSTpHandle_t *hdl) {
    assert(hdl);
    size_t len = 0;
    UDSTpGetRecvBuf(hdl, &len);
    return len;
}

void UDSTpAckRecv(UDSTpHandle_t *hdl) {
    assert(hdl);
    hdl->ack_recv(hdl);
}

// ========================================================================
//                              Common
// ========================================================================

#if UDS_CUSTOM_MILLIS
#else
uint32_t UDSMillis(void) {
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

static inline uint8_t NegativeResponse(UDSReq_t *r, uint8_t response_code) {
    r->send_buf[0] = 0x7F;
    r->send_buf[1] = r->recv_buf[0];
    r->send_buf[2] = response_code;
    r->send_len = UDS_NEG_RESP_LEN;
    return response_code;
}

static inline void NoResponse(UDSReq_t *r) { r->send_len = 0; }

static uint8_t EmitEvent(UDSServer_t *srv, UDSServerEvent_t evt, void *data) {
    if (srv->fn) {
        return srv->fn(srv, evt, data);
    } else {
        UDS_DBG_PRINT("Unhandled UDSServerEvent %d, srv.fn not installed!\n", evt);
        return kGeneralReject;
    }
}


static uint8_t _0x10_DiagnosticSessionControl(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X10_REQ_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t sessType = r->recv_buf[1] & 0x4F;

    UDSDiagSessCtrlArgs_t args = {
        .type = sessType,
        .p2_ms = UDS_CLIENT_DEFAULT_P2_MS,
        .p2_star_ms = UDS_CLIENT_DEFAULT_P2_STAR_MS,
    };

    uint8_t err = EmitEvent(srv, UDS_SRV_EVT_DiagSessCtrl, &args);

    if (kPositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    srv->sessionType = sessType;

    switch (sessType) {
    case kDefaultSession:
        break;
    case kProgrammingSession:
    case kExtendedDiagnostic:
    default:
        srv->s3_session_timeout_timer = UDSMillis() + srv->s3_ms;
        break;
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_DIAGNOSTIC_SESSION_CONTROL);
    r->send_buf[1] = sessType;

    // UDS-1-2013: Table 29
    // resolution: 1ms
    r->send_buf[2] = args.p2_ms >> 8;
    r->send_buf[3] = args.p2_ms;

    // resolution: 10ms
    r->send_buf[4] = (args.p2_star_ms / 10) >> 8;
    r->send_buf[5] = args.p2_star_ms / 10;

    r->send_len = UDS_0X10_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x11_ECUReset(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t resetType = r->recv_buf[1] & 0x3F;

    if (r->recv_len < UDS_0X11_REQ_MIN_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    UDSECUResetArgs_t args = {
        .type = resetType,
        .powerDownTimeMillis = UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS,
    };

    uint8_t err = EmitEvent(srv, UDS_SRV_EVT_EcuReset, &args);

    if (kPositiveResponse == err) {
        srv->notReadyToReceive = true;
        srv->ecuResetScheduled = resetType;
        srv->ecuResetTimer = UDSMillis() + args.powerDownTimeMillis;
    } else {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ECU_RESET);
    r->send_buf[1] = resetType;

    if (kEnableRapidPowerShutDown == resetType) {
        uint32_t powerDownTime = args.powerDownTimeMillis / 1000;
        if (powerDownTime > 255) {
            powerDownTime = 255;
        }
        r->send_buf[2] = powerDownTime;
        r->send_len = UDS_0X11_RESP_BASE_LEN + 1;
    } else {
        r->send_len = UDS_0X11_RESP_BASE_LEN;
    }
    return kPositiveResponse;
}

static uint8_t safe_copy(UDSServer_t *srv, const void *src, uint16_t count) {
    if (srv == NULL) {
        return kGeneralReject;
    }
    UDSReq_t *r = (UDSReq_t *)&srv->r;
    if (count <= r->send_buf_size - r->send_len) {
        memmove(r->send_buf + r->send_len, src, count);
        r->send_len += count;
        return kPositiveResponse;
    }
    return kResponseTooLong;
}

static uint8_t _0x22_ReadDataByIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t numDIDs;
    uint16_t dataId = 0;
    uint8_t ret = kPositiveResponse;
    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_DATA_BY_IDENTIFIER);
    r->send_len = 1;

    if (0 != (r->recv_len - 1) % sizeof(uint16_t)) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    numDIDs = r->recv_len / sizeof(uint16_t);

    if (0 == numDIDs) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (int did = 0; did < numDIDs; did++) {
        uint16_t idx = 1 + did * 2;
        dataId = (r->recv_buf[idx] << 8) + r->recv_buf[idx + 1];

        if (r->send_len + 3 > r->send_buf_size) {
            return NegativeResponse(r, kResponseTooLong);
        }
        uint8_t *copylocation = r->send_buf + r->send_len;
        copylocation[0] = dataId >> 8;
        copylocation[1] = dataId;
        r->send_len += 2;

        UDSRDBIArgs_t args = {
            .dataId = dataId,
            .copy = safe_copy,
        };

        ret = EmitEvent(srv, UDS_SRV_EVT_ReadDataByIdent, &args);

        if (kPositiveResponse != ret) {
            return NegativeResponse(r, ret);
        }
    }
    return kPositiveResponse;
}

/**
 * @brief decode the addressAndLengthFormatIdentifier that appears in ReadMemoryByAddress (0x23),
 * DynamicallyDefineDataIdentifier (0x2C), RequestDownload (0X34)
 *
 * @param srv
 * @param buf pointer to addressAndDataLengthFormatIdentifier in recv_buf
 * @param memoryAddress the decoded memory address
 * @param memorySize the decoded memory size
 * @return uint8_t
 */
static uint8_t decodeAddressAndLength(UDSReq_t *r, uint8_t *const buf, void **memoryAddress,
                                      size_t *memorySize) {
    assert(r);
    assert(memoryAddress);
    assert(memorySize);
    long long unsigned int tmp = 0;
    *memoryAddress = 0;
    *memorySize = 0;

    assert(buf >= r->recv_buf && buf <= r->recv_buf + sizeof(r->recv_buf));

    if (r->recv_len < 3) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t memorySizeLength = (buf[0] & 0xF0) >> 4;
    uint8_t memoryAddressLength = buf[0] & 0x0F;

    if (memorySizeLength == 0 || memorySizeLength > sizeof(size_t)) {
        return NegativeResponse(r, kRequestOutOfRange);
    }

    if (memoryAddressLength == 0 || memoryAddressLength > sizeof(size_t)) {
        return NegativeResponse(r, kRequestOutOfRange);
    }

    if (buf + memorySizeLength + memoryAddressLength > r->recv_buf + r->recv_len) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
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

static uint8_t _0x23_ReadMemoryByAddress(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t ret = kPositiveResponse;
    void *address = 0;
    size_t length = 0;

    if (r->recv_len < UDS_0X23_REQ_MIN_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    ret = decodeAddressAndLength(r, &r->recv_buf[1], &address, &length);
    if (kPositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    UDSReadMemByAddrArgs_t args = {
        .memAddr = address,
        .memSize = length,
        .copy = safe_copy,
    };

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_MEMORY_BY_ADDRESS);
    r->send_len = UDS_0X23_RESP_BASE_LEN;
    ret = EmitEvent(srv, UDS_SRV_EVT_ReadMemByAddr, &args);
    if (kPositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }
    if (r->send_len != UDS_0X23_RESP_BASE_LEN + length) {
        return kGeneralProgrammingFailure;
    }
    return kPositiveResponse;
}

static uint8_t _0x27_SecurityAccess(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t subFunction = r->recv_buf[1];
    uint8_t response = kPositiveResponse;

    if (UDSSecurityAccessLevelIsReserved(subFunction)) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_SECURITY_ACCESS);
    r->send_buf[1] = subFunction;
    r->send_len = UDS_0X27_RESP_BASE_LEN;

    // Even: sendKey
    if (0 == subFunction % 2) {
        uint8_t requestedLevel = subFunction - 1;
        UDSSecAccessValidateKeyArgs_t args = {
            .level = requestedLevel,
            .key = &r->recv_buf[UDS_0X27_REQ_BASE_LEN],
            .len = r->recv_len - UDS_0X27_REQ_BASE_LEN,
        };

        response = EmitEvent(srv, UDS_SRV_EVT_SecAccessValidateKey, &args);

        if (kPositiveResponse != response) {
            return NegativeResponse(r, response);
        }

        // "requestSeed = 0x01" identifies a fixed relationship between
        // "requestSeed = 0x01" and "sendKey = 0x02"
        // "requestSeed = 0x03" identifies a fixed relationship between
        // "requestSeed = 0x03" and "sendKey = 0x04"
        srv->securityLevel = requestedLevel;
        r->send_len = UDS_0X27_RESP_BASE_LEN;
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
        if (subFunction == srv->securityLevel) {
            // Table 52 sends a response of length 2. Use a preprocessor define if this needs
            // customizing by the user.
            const uint8_t already_unlocked[] = {0x00, 0x00};
            return safe_copy(srv, already_unlocked, sizeof(already_unlocked));
        } else {
            UDSSecAccessRequestSeedArgs_t args = {
                .level = subFunction,
                .dataRecord = &r->recv_buf[UDS_0X27_REQ_BASE_LEN],
                .len = r->recv_len - UDS_0X27_REQ_BASE_LEN,
                .copySeed = safe_copy,
            };

            response = EmitEvent(srv, UDS_SRV_EVT_SecAccessRequestSeed, &args);

            if (kPositiveResponse != response) {
                return NegativeResponse(r, response);
            }

            if (r->send_len <= UDS_0X27_RESP_BASE_LEN) { // no data was copied
                return NegativeResponse(r, kGeneralProgrammingFailure);
            }
            return kPositiveResponse;
        }
    }
    return NegativeResponse(r, kGeneralProgrammingFailure);
}

static uint8_t _0x28_CommunicationControl(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t controlType = r->recv_buf[1] & 0x7F;
    uint8_t communicationType = r->recv_buf[2];

    if (r->recv_len < UDS_0X28_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    UDSCommCtrlArgs_t args = {
        .ctrlType = controlType,
        .commType = communicationType,
    };

    uint8_t err = EmitEvent(srv, UDS_SRV_EVT_CommCtrl, &args);
    if (kPositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_COMMUNICATION_CONTROL);
    r->send_buf[1] = controlType;
    r->send_len = UDS_0X28_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x2E_WriteDataByIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    uint16_t dataLen = 0;
    uint16_t dataId = 0;
    uint8_t err = kPositiveResponse;

    /* UDS-1 2013 Figure 21 Key 1 */
    if (r->recv_len < UDS_0X2E_REQ_MIN_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    dataId = (r->recv_buf[1] << 8) + r->recv_buf[2];
    dataLen = r->recv_len - UDS_0X2E_REQ_BASE_LEN;

    UDSWDBIArgs_t args = {
        .dataId = dataId,
        .data = &r->recv_buf[UDS_0X2E_REQ_BASE_LEN],
        .len = dataLen,
    };

    err = EmitEvent(srv, UDS_SRV_EVT_WriteDataByIdent, &args);
    if (kPositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_WRITE_DATA_BY_IDENTIFIER);
    r->send_buf[1] = dataId >> 8;
    r->send_buf[2] = dataId;
    r->send_len = UDS_0X2E_RESP_LEN;
    return kPositiveResponse;
}

static uint8_t _0x31_RoutineControl(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t err = kPositiveResponse;
    if (r->recv_len < UDS_0X31_REQ_MIN_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t routineControlType = r->recv_buf[1] & 0x7F;
    uint16_t routineIdentifier = (r->recv_buf[2] << 8) + r->recv_buf[3];

    UDSRoutineCtrlArgs_t args = {
        .ctrlType = routineControlType,
        .id = routineIdentifier,
        .optionRecord = &r->recv_buf[UDS_0X31_REQ_MIN_LEN],
        .len = r->recv_len - UDS_0X31_REQ_MIN_LEN,
        .copyStatusRecord = safe_copy,
    };

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL);
    r->send_buf[1] = routineControlType;
    r->send_buf[2] = routineIdentifier >> 8;
    r->send_buf[3] = routineIdentifier;
    r->send_len = UDS_0X31_RESP_MIN_LEN;

    switch (routineControlType) {
    case kStartRoutine:
    case kStopRoutine:
    case kRequestRoutineResults:
        err = EmitEvent(srv, UDS_SRV_EVT_RoutineCtrl, &args);
        if (kPositiveResponse != err) {
            return NegativeResponse(r, err);
        }
        break;
    default:
        return NegativeResponse(r, kRequestOutOfRange);
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

static uint8_t _0x34_RequestDownload(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t err;
    void *memoryAddress = 0;
    size_t memorySize = 0;

    if (srv->xferIsActive) {
        return NegativeResponse(r, kConditionsNotCorrect);
    }

    if (r->recv_len < UDS_0X34_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    err = decodeAddressAndLength(r, &r->recv_buf[2], &memoryAddress, &memorySize);
    if (kPositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    UDSRequestDownloadArgs_t args = {
        .addr = memoryAddress,
        .size = memorySize,
        .dataFormatIdentifier = r->recv_buf[1],
        .maxNumberOfBlockLength = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH,
    };

    err = EmitEvent(srv, UDS_SRV_EVT_RequestDownload, &args);

    if (args.maxNumberOfBlockLength < 3) {
        UDS_DBG_PRINT("ERROR: maxNumberOfBlockLength too short");
        return NegativeResponse(r, kGeneralProgrammingFailure);
    }

    if (kPositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    ResetTransfer(srv);
    srv->xferIsActive = true;
    srv->xferTotalBytes = memorySize;
    srv->xferBlockLength = args.maxNumberOfBlockLength;

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

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_DOWNLOAD);
    r->send_buf[1] = lengthFormatIdentifier;
    for (uint8_t idx = 0; idx < sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = sizeof(args.maxNumberOfBlockLength) - 1 - idx;
        uint8_t byte = args.maxNumberOfBlockLength >> (shiftBytes * 8);
        r->send_buf[UDS_0X34_RESP_BASE_LEN + idx] = byte;
    }
    r->send_len = UDS_0X34_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength);
    return kPositiveResponse;
}

static uint8_t _0x35_RequestUpload(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t err;
    void *memoryAddress = 0;
    size_t memorySize = 0;

    if (srv->xferIsActive) {
        return NegativeResponse(r, kConditionsNotCorrect);
    }

    if (r->recv_len < UDS_0X35_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    err = decodeAddressAndLength(r, &r->recv_buf[2], &memoryAddress, &memorySize);
    if (kPositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    UDSRequestUploadArgs_t args = {
        .addr = memoryAddress,
        .size = memorySize,
        .dataFormatIdentifier = r->recv_buf[1],
        .maxNumberOfBlockLength = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH,
    };

    err = EmitEvent(srv, UDS_SRV_EVT_RequestUpload, &args);

    if (args.maxNumberOfBlockLength < 3) {
        UDS_DBG_PRINT("ERROR: maxNumberOfBlockLength too short");
        return NegativeResponse(r, kGeneralProgrammingFailure);
    }

    if (kPositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    ResetTransfer(srv);
    srv->xferIsActive = true;
    srv->xferTotalBytes = memorySize;
    srv->xferBlockLength = args.maxNumberOfBlockLength;

    uint8_t lengthFormatIdentifier = sizeof(args.maxNumberOfBlockLength) << 4;

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_UPLOAD);
    r->send_buf[1] = lengthFormatIdentifier;
    for (uint8_t idx = 0; idx < sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = sizeof(args.maxNumberOfBlockLength) - 1 - idx;
        uint8_t byte = args.maxNumberOfBlockLength >> (shiftBytes * 8);
        r->send_buf[UDS_0X35_RESP_BASE_LEN + idx] = byte;
    }
    r->send_len = UDS_0X35_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength);
    return kPositiveResponse;
}

static uint8_t _0x36_TransferData(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t err = kPositiveResponse;
    uint16_t request_data_len = r->recv_len - UDS_0X36_REQ_BASE_LEN;
    uint8_t blockSequenceCounter = 0;

    if (!srv->xferIsActive) {
        return NegativeResponse(r, kUploadDownloadNotAccepted);
    }

    if (r->recv_len < UDS_0X36_REQ_BASE_LEN) {
        err = kIncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    blockSequenceCounter = r->recv_buf[1];

    if (!srv->RCRRP) {
        if (blockSequenceCounter != srv->xferBlockSequenceCounter) {
            err = kRequestSequenceError;
            goto fail;
        } else {
            srv->xferBlockSequenceCounter++;
        }
    }

    if (srv->xferByteCounter + request_data_len > srv->xferTotalBytes) {
        err = kTransferDataSuspended;
        goto fail;
    }

    {
        UDSTransferDataArgs_t args = {
            .data = &r->recv_buf[UDS_0X36_REQ_BASE_LEN],
            .len = r->recv_len - UDS_0X36_REQ_BASE_LEN,
            .maxRespLen = srv->xferBlockLength - UDS_0X36_RESP_BASE_LEN,
            .copyResponse = safe_copy,
        };

        r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TRANSFER_DATA);
        r->send_buf[1] = blockSequenceCounter;
        r->send_len = UDS_0X36_RESP_BASE_LEN;

        err = EmitEvent(srv, UDS_SRV_EVT_TransferData, &args);

        switch (err) {
        case kPositiveResponse:
            srv->xferByteCounter += request_data_len;
            return kPositiveResponse;
        case kRequestCorrectlyReceived_ResponsePending:
            return NegativeResponse(r, kRequestCorrectlyReceived_ResponsePending);
        default:
            goto fail;
        }
    }

fail:
    ResetTransfer(srv);
    return NegativeResponse(r, err);
}

static uint8_t _0x37_RequestTransferExit(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t err = kPositiveResponse;

    if (!srv->xferIsActive) {
        return NegativeResponse(r, kUploadDownloadNotAccepted);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_TRANSFER_EXIT);
    r->send_len = UDS_0X37_RESP_BASE_LEN;

    UDSRequestTransferExitArgs_t args = {
        .data = &r->recv_buf[UDS_0X37_REQ_BASE_LEN],
        .len = r->recv_len - UDS_0X37_REQ_BASE_LEN,
        .copyResponse = safe_copy,
    };

    err = EmitEvent(srv, UDS_SRV_EVT_RequestTransferExit, &args);

    switch (err) {
    case kPositiveResponse:
        ResetTransfer(srv);
        return kPositiveResponse;
    case kRequestCorrectlyReceived_ResponsePending:
        return NegativeResponse(r, kRequestCorrectlyReceived_ResponsePending);
    default:
        ResetTransfer(srv);
        return NegativeResponse(r, err);
    }
}

static uint8_t _0x3E_TesterPresent(UDSServer_t *srv, UDSReq_t *r) {
    if ((r->recv_len < UDS_0X3E_REQ_MIN_LEN) ||
        (r->recv_len > UDS_0X3E_REQ_MAX_LEN)) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }
    uint8_t zeroSubFunction = r->recv_buf[1];

    switch (zeroSubFunction) {
    case 0x00:
    case 0x80:
        srv->s3_session_timeout_timer = UDSMillis() + srv->s3_ms;
        r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TESTER_PRESENT);
        r->send_buf[1] = 0x00;
        r->send_len = UDS_0X3E_RESP_LEN;
        return kPositiveResponse;
    default:
        return NegativeResponse(r, kSubFunctionNotSupported);
    }
}

static uint8_t _0x85_ControlDTCSetting(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X85_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }
    uint8_t dtcSettingType = r->recv_buf[1] & 0x3F;

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_CONTROL_DTC_SETTING);
    r->send_buf[1] = dtcSettingType;
    r->send_len = UDS_0X85_RESP_LEN;
    return kPositiveResponse;
}

typedef uint8_t (*UDSService)(UDSServer_t *srv, UDSReq_t *r);

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
 * @param srv
 * @param addressingScheme
 */
static uint8_t evaluateServiceResponse(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t response = kPositiveResponse;
    bool suppressResponse = false;
    uint8_t sid = r->recv_buf[0];
    UDSService service = getServiceForSID(sid);

    if (NULL == service || NULL == srv->fn) {
        return NegativeResponse(r, kServiceNotSupported);
    }
    assert(service);
    assert(srv->fn); // service handler functions will call srv->fn. it must be valid

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
        response = service(srv, r);

        bool suppressPosRspMsgIndicationBit = r->recv_buf[1] & 0x80;

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
        response = service(srv, r);
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

    if ((UDS_A_TA_TYPE_FUNCTIONAL == r->info.A_TA_Type) &&
        ((kServiceNotSupported == response) || (kSubFunctionNotSupported == response) ||
         (kServiceNotSupportedInActiveSession == response) ||
         (kSubFunctionNotSupportedInActiveSession == response) ||
         (kRequestOutOfRange == response)) &&
        (
            // TODO: *not yet a NRC 0x78 response sent*
            true)) {
        suppressResponse = true; /* Suppress negative response message */
        NoResponse(r);
    } else {
        if (suppressResponse) { /* Suppress positive response message */
            NoResponse(r);
        } else { /* send negative or positive response */
            ;
        }
    }
    return response;
}

// ========================================================================
//                             Public Functions
// ========================================================================

/**
 * @brief \~chinese 初始化服务器 \~english Initialize the server
 *
 * @param srv
 * @param cfg
 * @return int
 */
UDSErr_t UDSServerInit(UDSServer_t *srv) {
    assert(srv);
    memset(srv, 0, sizeof(UDSServer_t));
    srv->p2_ms = UDS_SERVER_DEFAULT_P2_MS;
    srv->p2_star_ms = UDS_SERVER_DEFAULT_P2_STAR_MS;
    srv->s3_ms = UDS_SERVER_DEFAULT_S3_MS;
    srv->sessionType = kDefaultSession;
    srv->p2_timer = UDSMillis() + srv->p2_ms;
    srv->s3_session_timeout_timer = UDSMillis() + srv->s3_ms;
    return UDS_OK;
}

void UDSServerPoll(UDSServer_t *srv) {
    // UDS-1-2013 Figure 38: Session Timeout (S3)
    if (kDefaultSession != srv->sessionType &&
        UDSTimeAfter(UDSMillis(), srv->s3_session_timeout_timer)) {
        EmitEvent(srv, UDS_SRV_EVT_SessionTimeout, NULL);
    }

    if (srv->ecuResetScheduled && UDSTimeAfter(UDSMillis(), srv->ecuResetTimer)) {
        EmitEvent(srv, UDS_SRV_EVT_DoScheduledReset, &srv->ecuResetScheduled);
    }

    UDSTpStatus_t tpStatus = UDSTpPoll(srv->tp);

    UDSReq_t *r = &srv->r;

    if (srv->requestInProgress) {
        if (srv->RCRRP) {
            // responds only if 
            // 1. changed (no longer RCRRP), or 
            // 2. p2_timer has elapsed
            uint8_t response = evaluateServiceResponse(srv, r);
            if (kRequestCorrectlyReceived_ResponsePending == response) {
                // it's the second time the service has responded with RCRRP
                srv->notReadyToReceive = true;
            } else {
                // No longer RCRRP'ing
                srv->RCRRP = false;
                srv->notReadyToReceive = false;

                // Not a consecutive 0x78 response, use p2 instead of p2_star * 0.3
                srv->p2_timer = UDSMillis() + srv->p2_ms;
            }
        }

        if (UDSTimeAfter(UDSMillis(), srv->p2_timer)) {
            printf("len: %ld\n", r->send_len);
            ssize_t ret = UDSTpSend(srv->tp, r->send_buf, r->send_len, NULL);
            // TODO test injection of transport errors:
            if (ret < 0) {
                UDSErr_t err  = UDS_ERR_TPORT;
                EmitEvent(srv, UDS_SRV_EVT_Err, &err);
                UDS_DBG_PRINT("UDSTpSend failed with %ld\n", ret);
            }

            if (srv->RCRRP) {
                // ISO14229-2:2013 Table 4 footnote b 
                // min time between consecutive 0x78 responses is 0.3 * p2*
                uint32_t wait_time = srv->p2_star_ms * 3 / 10;
                srv->p2_timer = UDSMillis() + wait_time;
            } else {
                srv->p2_timer = UDSMillis() + srv->p2_ms;
                UDSTpAckRecv(srv->tp);
                srv->requestInProgress = false;
            }
        }

    } else {
        if (srv->notReadyToReceive) {
            return; // cannot respond to request right now
        } 
        r->recv_len = UDSTpPeek(srv->tp, &r->recv_buf, &r->info);
        r->send_buf_size = UDSTpGetSendBuf(srv->tp, &r->send_buf);
        if (r->send_buf == NULL || r->recv_buf == NULL) {
            UDSErr_t err  = UDS_ERR_TPORT;
            EmitEvent(srv, UDS_SRV_EVT_Err, &err);
            UDS_DBG_PRINT("bad tport\n");
            return;
        }
        if (r->recv_len > 0) {
            uint8_t response = evaluateServiceResponse(srv, r);
            srv->requestInProgress = true;
            if (kRequestCorrectlyReceived_ResponsePending == response) {
                srv->RCRRP = true;
            }
        }
    }
}


// ========================================================================
//                              Client
// ========================================================================

static void clearRequestContext(UDSClient_t *client) {
    assert(client);
    client->recv_size = 0;
    client->send_size = 0;
    client->state = kRequestStateIdle;
    client->err = UDS_OK;
}

UDSErr_t UDSClientInit(UDSClient_t *client) {
    assert(client);
    memset(client, 0, sizeof(*client));

    client->p2_ms = UDS_CLIENT_DEFAULT_P2_MS;
    client->p2_star_ms = UDS_CLIENT_DEFAULT_P2_STAR_MS;

    if (client->p2_star_ms < client->p2_ms) {
        fprintf(stderr, "p2_star_ms must be >= p2_ms\n");
        client->p2_star_ms = client->p2_ms;
    }

    clearRequestContext(client);
    return UDS_OK;
}

static const char *ClientStateName(enum UDSClientRequestState state) {
    switch (state) {
    case kRequestStateIdle:
        return "Idle";
    case kRequestStateSending:
        return "Sending";
        case kRequestStateAwaitSendComplete:
        return "AwaitSendComplete";
    case kRequestStateAwaitResponse:
        return "AwaitResponse";
        case kRequestStateProcessResponse:
        return "ProcessResponse";
    default:
        return "Unknown";
    }
}

static void changeState(UDSClient_t *client, enum UDSClientRequestState state) {
    printf("client state: %s (%d) -> %s (%d)\n", ClientStateName(client->state), client->state, ClientStateName(state), state);
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
            UDSTpAckRecv(client->tp);
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
                UDSTpAckRecv(client->tp);
                changeState(client, kRequestStateIdle);
                return;
            }

            if (client->_options_copy & UDS_IGNORE_SRV_TIMINGS) {
                UDSTpAckRecv(client->tp);
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
    UDSTpAckRecv(client->tp);
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
            client->_options_copy & UDS_FUNCTIONAL ? UDS_A_TA_TYPE_FUNCTIONAL : UDS_A_TA_TYPE_PHYSICAL;
        UDSSDU_t info = {
            .A_Mtype = UDS_A_MTYPE_DIAG,
            .A_TA_Type = ta_type,
        };
        ssize_t ret = UDSTpSend(client->tp, client->send_buf, client->send_size, &info);
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
        UDSSDU_t info = {0};
        ssize_t len = UDSTpPeek(client->tp, &client->recv_buf, &info);

        if (UDS_A_TA_TYPE_FUNCTIONAL == info.A_TA_Type) {
            UDSTpAckRecv(client->tp);
            break;
        }
        if (len < 0) {
            client->err = UDS_ERR_TPORT;
            changeState(client, kRequestStateIdle);
        } else if (0 == len) {
            if (UDSTimeAfter(UDSMillis(), client->p2_timer)) {
                client->err = UDS_ERR_TIMEOUT;
                changeState(client, kRequestStateIdle);
            }
        } else {
            printf("received %ld bytes\n", len);
            client->recv_size = len;
            changeState(client, kRequestStateProcessResponse);
        }
        break;
    }
    case kRequestStateProcessResponse: {
        client->err = _ClientValidateResponse(client);
        if (UDS_OK == client->err) {
            _ClientHandleResponse(client);
        } else {
            UDSTpAckRecv(client->tp);
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


static UDSErr_t PreRequestCheck(UDSClient_t *client) {
    if (kRequestStateIdle != client->state) {
        return UDS_ERR_BUSY;
    }
    clearRequestContext(client);
    if (client->tp == NULL) {
        return UDS_ERR_TPORT;
    }
    ssize_t ret = UDSTpGetSendBuf(client->tp, &client->send_buf);
    if (ret < 0) {
        return UDS_ERR_TPORT;
    }
    client->send_buf_size = ret;
    return UDS_OK;
}

UDSErr_t UDSSendBytes(UDSClient_t *client, const uint8_t *data, uint16_t size) {
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
    if (size > client->send_buf_size) {
        return UDS_ERR_BUFSIZ;
    }
    memmove(client->send_buf, data, size);
    client->send_size = size;
    return _SendRequest(client);
}

UDSErr_t UDSSendECUReset(UDSClient_t *client, UDSECUReset_t type) {
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
    client->send_buf[0] = kSID_ECU_RESET;
    client->send_buf[1] = type;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSErr_t UDSSendDiagSessCtrl(UDSClient_t *client, enum UDSDiagnosticSessionType mode) {
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
    client->send_buf[0] = kSID_DIAGNOSTIC_SESSION_CONTROL;
    client->send_buf[1] = mode;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSErr_t UDSSendCommCtrl(UDSClient_t *client, enum UDSCommunicationControlType ctrl,
                         enum UDSCommunicationType comm) {
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
    client->send_buf[0] = kSID_COMMUNICATION_CONTROL;
    client->send_buf[1] = ctrl;
    client->send_buf[2] = comm;
    client->send_size = 3;
    return _SendRequest(client);
}

UDSErr_t UDSSendTesterPresent(UDSClient_t *client) {
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
    client->send_buf[0] = kSID_TESTER_PRESENT;
    client->send_buf[1] = 0;
    client->send_size = 2;
    return _SendRequest(client);
}

UDSErr_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                     const uint16_t numDataIdentifiers) {
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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
    UDSErr_t err = PreRequestCheck(client);
if (err) {
    return err;
}
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

UDSErr_t UDSUnpackRDBIResponse(const uint8_t *buf, size_t buf_len, uint16_t did, uint8_t *data,
                               uint16_t data_size, uint16_t *offset) {
    assert(buf);
    assert(data);
    assert(offset);
    if (0 == *offset) {
        *offset = UDS_0X22_RESP_BASE_LEN;
    }

    if (*offset + sizeof(did) > buf_len) {
        return UDS_ERR_RESP_TOO_SHORT;
    }

    uint16_t theirDID = (buf[*offset] << 8) + buf[*offset + 1];
    if (theirDID != did) {
        return UDS_ERR_DID_MISMATCH;
    }

    if (*offset + sizeof(uint16_t) + data_size > buf_len) {
        return UDS_ERR_RESP_TOO_SHORT;
    }

    memmove(data, buf + *offset + sizeof(uint16_t), data_size);

    *offset += sizeof(uint16_t) + data_size;
    return UDS_OK;
}
