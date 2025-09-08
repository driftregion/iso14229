#include "server.h"
#include "config.h"
#include "uds.h"
#include "util.h"
#include "log.h"
#include <stdint.h>

static inline UDSErr_t NegativeResponse(UDSReq_t *r, UDSErr_t nrc) {
    if (nrc < 0 || nrc > 0xFF) {
        UDS_LOGW(__FILE__, "Invalid negative response code: %d (0x%x)", nrc, nrc);
        nrc = UDS_NRC_GeneralReject;
    }

    r->send_buf[0] = 0x7F;
    r->send_buf[1] = r->recv_buf[0];
    r->send_buf[2] = (uint8_t)nrc;
    r->send_len = UDS_NEG_RESP_LEN;
    return nrc;
}

static inline void NoResponse(UDSReq_t *r) { r->send_len = 0; }

static UDSErr_t EmitEvent(UDSServer_t *srv, UDSEvent_t evt, void *data) {
    UDSErr_t err = UDS_OK;
    if (srv->fn) {
        err = srv->fn(srv, evt, data);
    } else {
        UDS_LOGI(__FILE__, "Unhandled UDSEvent %d, srv.fn not installed!\n", evt);
        err = UDS_NRC_GeneralReject;
    }
    if (!UDSErrIsNRC(err)) {
        UDS_LOGW(__FILE__, "The returned error code %d (0x%x) is not a negative response code", err,
                 err);
    }
    return err;
}

static UDSErr_t Handle_0x10_DiagnosticSessionControl(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X10_REQ_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t sessType = r->recv_buf[1] & 0x7F;

    UDSDiagSessCtrlArgs_t args = {
        .type = sessType,
        .p2_ms = UDS_CLIENT_DEFAULT_P2_MS,
        .p2_star_ms = UDS_CLIENT_DEFAULT_P2_STAR_MS,
    };

    UDSErr_t err = EmitEvent(srv, UDS_EVT_DiagSessCtrl, &args);

    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    srv->sessionType = sessType;

    switch (sessType) {
    case UDS_LEV_DS_DS: // default session
        break;
    case UDS_LEV_DS_PRGS:  // programming session
    case UDS_LEV_DS_EXTDS: // extended diagnostic session
    default:
        srv->s3_session_timeout_timer = UDSMillis() + srv->s3_ms;
        break;
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_DIAGNOSTIC_SESSION_CONTROL);
    r->send_buf[1] = sessType;

    // UDS-1-2013: Table 29
    // resolution: 1ms
    r->send_buf[2] = args.p2_ms >> 8;
    r->send_buf[3] = args.p2_ms & 0xFF;

    // resolution: 10ms
    r->send_buf[4] = (uint8_t)((args.p2_star_ms / 10) >> 8);
    r->send_buf[5] = (uint8_t)(args.p2_star_ms / 10);

    r->send_len = UDS_0X10_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x11_ECUReset(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t resetType = r->recv_buf[1] & 0x3F;

    if (r->recv_len < UDS_0X11_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    UDSECUResetArgs_t args = {
        .type = resetType,
        .powerDownTimeMillis = UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS,
    };

    UDSErr_t err = EmitEvent(srv, UDS_EVT_EcuReset, &args);

    if (UDS_PositiveResponse == err) {
        srv->notReadyToReceive = true;
        srv->ecuResetScheduled = resetType;
        srv->ecuResetTimer = UDSMillis() + args.powerDownTimeMillis;
    } else {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ECU_RESET);
    r->send_buf[1] = resetType;

    if (UDS_LEV_RT_ERPSD == resetType) {
        uint32_t powerDownTime = args.powerDownTimeMillis / 1000;
        if (powerDownTime > 255) {
            powerDownTime = 255;
        }
        r->send_buf[2] = powerDownTime & 0xFF;
        r->send_len = UDS_0X11_RESP_BASE_LEN + 1;
    } else {
        r->send_len = UDS_0X11_RESP_BASE_LEN;
    }
    return UDS_PositiveResponse;
}

static uint8_t safe_copy(UDSServer_t *srv, const void *src, uint16_t count) {
    if (srv == NULL) {
        return UDS_NRC_GeneralReject;
    }
    if (src == NULL) {
        return UDS_NRC_GeneralReject;
    }
    UDSReq_t *r = (UDSReq_t *)&srv->r;
    if (count <= sizeof(r->send_buf) - r->send_len) {
        memmove(r->send_buf + r->send_len, src, count);
        r->send_len += count;
        return UDS_PositiveResponse;
    }
    return UDS_NRC_ResponseTooLong;
}

static UDSErr_t Handle_0x19_ReadDTCInformation(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    uint8_t type = r->recv_buf[1];

    if (r->recv_len < UDS_0X19_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    // Shared by all SubFunc
    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_DTC_INFORMATION);
    r->send_buf[1] = type;
    r->send_len = UDS_0X19_RESP_BASE_LEN;

    UDSRDTCIArgs_t args = {
        .type = type,
        .copy = safe_copy,
    };

    // Before checks and emitting Request
    switch (type) {
    case 0x01: // reportNumberOfDTCByStatusMask
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 1) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.reportNumberOfDTCByStatusMaskArgs.mask = r->recv_buf[2];
        break;
    case 0x02: // reportDTCByStatusMask
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 1) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.reportDTCStatusByMaskArgs.mask = r->recv_buf[2];
        break;
    case 0x03: // reportDTCSnapshotIdentification
        // has no subfunction specific args
        break;
    case 0x04: // reportDTCSnapshotRecordByDTCNumber
    case 0x05: // reportDTCStoredDataByRecordNumber
    case 0x06: // reportDTCExtDataRecordByDTCNumber
    case 0x07: // reportNumberOfDTCBySeverityMaskRecord
    case 0x08: // reportDTCBySeverityMaskRecord
    case 0x09: // reportSeverityInformationOfDTC
    case 0x0A: // reportSupportedDTC
    case 0x0B: // reportFirstTestFailedDTC
    case 0x0C: // reportFirstConfirmedDTC
    case 0x0D: // reportMostRecentTestFailedDTC
    case 0x0E: // reportMostRecentConfirmedDTC
    case 0x14: // reportDTCFaultDetectionCounter
    case 0x15: // reportDTCWithPermanentStatus
    case 0x16: // reportDTCExtDataRecordByNumber
    case 0x17: // reportUserDefMemoryDTCByStatusMask
    case 0x18: // reportUserDefMemoryDTCSnapshotRecordByDTCNumber
    case 0x19: // reportUserDefMemoryDTCExtDAtaRecordByDTCNumber
    case 0x1A: // reportDTCExtendedDataRecordIdentification
    case 0x42: // reportWWHOBDDTCByMaskRecord
    case 0x55: // reportWWHOBDDTCWithPermanentStatus
    case 0x56: // reportDTCInformationByDTCReadinessGroupIdentifier

    default:
        return UDS_NRC_SubFunctionNotSupported;
    }

    ret = EmitEvent(srv, UDS_EVT_ReadDTCInformation, &args);

    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    if (r->send_len < UDS_0X19_RESP_BASE_LEN) {
        return UDS_NRC_GeneralProgrammingFailure;
    }

    /* subfunc specific  reply len checks */
    switch (type) {
    case 0x01: /* reportNumberOfDTCByStatusMask */
        if (r->send_len != UDS_0X19_RESP_BASE_LEN + 4) {
            return UDS_NRC_GeneralProgrammingFailure;
        }
        break;
    case 0x02: /* reportDTCByStatusMask */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 1 ||
            (r->send_len - (UDS_0X19_RESP_BASE_LEN + 1)) % 4 != 0) {
            return UDS_NRC_GeneralProgrammingFailure;
        }
        break;
    case 0x03: /* reportDTCSnapshotIdentification */
        if ((r->send_len - UDS_0X19_RESP_BASE_LEN) % 4 != 0) {
            return UDS_NRC_GeneralProgrammingFailure;
        }
        break;
    case 0x04: // reportDTCSnapshotRecordByDTCNumber
    case 0x05: // reportDTCStoredDataByRecordNumber
    case 0x06: // reportDTCExtDataRecordByDTCNumber
    case 0x07: // reportNumberOfDTCBySeverityMaskRecord
    case 0x08: // reportDTCBySeverityMaskRecord
    case 0x09: // reportSeverityInformationOfDTC
    case 0x0A: // reportSupportedDTC
    case 0x0B: // reportFirstTestFailedDTC
    case 0x0C: // reportFirstConfirmedDTC
    case 0x0D: // reportMostRecentTestFailedDTC
    case 0x0E: // reportMostRecentConfirmedDTC
    case 0x14: // reportDTCFaultDetectionCounter
    case 0x15: // reportDTCWithPermanentStatus
    case 0x16: // reportDTCExtDataRecordByNumber
    case 0x17: // reportUserDefMemoryDTCByStatusMask
    case 0x18: // reportUserDefMemoryDTCSnapshotRecordByDTCNumber
    case 0x19: // reportUserDefMemoryDTCExtDAtaRecordByDTCNumber
    case 0x1A: // reportDTCExtendedDataRecordIdentification
    case 0x42: // reportWWHOBDDTCByMaskRecord
    case 0x55: // reportWWHOBDDTCWithPermanentStatus
    case 0x56: // reportDTCInformationByDTCReadinessGroupIdentifier
    default:
        return UDS_NRC_SubFunctionNotSupported;
    }

    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x22_ReadDataByIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t numDIDs;
    uint16_t dataId = 0;
    UDSErr_t ret = UDS_PositiveResponse;
    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_DATA_BY_IDENTIFIER);
    r->send_len = 1;

    if (0 != (r->recv_len - 1) % sizeof(uint16_t)) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    numDIDs = (uint8_t)(r->recv_len / sizeof(uint16_t));

    if (0 == numDIDs) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    for (uint16_t did = 0; did < numDIDs; did++) {
        uint16_t idx = (uint16_t)(1 + did * 2);
        dataId = (uint16_t)((uint16_t)(r->recv_buf[idx] << 8) | (uint16_t)r->recv_buf[idx + 1]);

        if (r->send_len + 3 > sizeof(r->send_buf)) {
            return NegativeResponse(r, UDS_NRC_ResponseTooLong);
        }
        uint8_t *copylocation = r->send_buf + r->send_len;
        copylocation[0] = dataId >> 8;
        copylocation[1] = dataId & 0xFF;
        r->send_len += 2;

        UDSRDBIArgs_t args = {
            .dataId = dataId,
            .copy = safe_copy,
        };

        unsigned send_len_before = r->send_len;
        ret = EmitEvent(srv, UDS_EVT_ReadDataByIdent, &args);
        if (ret == UDS_PositiveResponse && send_len_before == r->send_len) {
            UDS_LOGE(__FILE__, "RDBI response positive but no data sent\n");
            return NegativeResponse(r, UDS_NRC_GeneralReject);
        }

        if (UDS_PositiveResponse != ret) {
            return NegativeResponse(r, ret);
        }
    }
    return UDS_PositiveResponse;
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
static UDSErr_t decodeAddressAndLength(UDSReq_t *r, uint8_t *const buf, void **memoryAddress,
                                       size_t *memorySize) {
    UDS_ASSERT(r);
    UDS_ASSERT(memoryAddress);
    UDS_ASSERT(memorySize);
    uintptr_t tmp = 0;
    *memoryAddress = 0;
    *memorySize = 0;

    UDS_ASSERT(buf >= r->recv_buf && buf <= r->recv_buf + sizeof(r->recv_buf));

    if (r->recv_len < 3) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t memorySizeLength = (buf[0] & 0xF0) >> 4;
    uint8_t memoryAddressLength = buf[0] & 0x0F;

    if (memorySizeLength == 0 || memorySizeLength > sizeof(size_t)) {
        return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
    }

    if (memoryAddressLength == 0 || memoryAddressLength > sizeof(size_t)) {
        return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
    }

    if (buf + memorySizeLength + memoryAddressLength > r->recv_buf + r->recv_len) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    for (int byteIdx = 0; byteIdx < memoryAddressLength; byteIdx++) {
        long long unsigned int byte = buf[1 + byteIdx];
        uint8_t shiftBytes = (uint8_t)(memoryAddressLength - 1 - byteIdx);
        tmp |= byte << (8 * shiftBytes);
    }
    *memoryAddress = (void *)tmp;

    for (int byteIdx = 0; byteIdx < memorySizeLength; byteIdx++) {
        uint8_t byte = buf[1 + memoryAddressLength + byteIdx];
        uint8_t shiftBytes = (uint8_t)(memorySizeLength - 1 - byteIdx);
        *memorySize |= (size_t)byte << (8 * shiftBytes);
    }
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x23_ReadMemoryByAddress(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    void *address = 0;
    size_t length = 0;

    if (r->recv_len < UDS_0X23_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    ret = decodeAddressAndLength(r, &r->recv_buf[1], &address, &length);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    UDSReadMemByAddrArgs_t args = {
        .memAddr = address,
        .memSize = length,
        .copy = safe_copy,
    };

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_MEMORY_BY_ADDRESS);
    r->send_len = UDS_0X23_RESP_BASE_LEN;
    ret = EmitEvent(srv, UDS_EVT_ReadMemByAddr, &args);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }
    if (r->send_len != UDS_0X23_RESP_BASE_LEN + length) {
        return UDS_NRC_GeneralProgrammingFailure;
    }
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x27_SecurityAccess(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t subFunction = r->recv_buf[1];
    UDSErr_t response = UDS_PositiveResponse;

    if (UDSSecurityAccessLevelIsReserved(subFunction)) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    if (!UDSTimeAfter(UDSMillis(), srv->sec_access_boot_delay_timer)) {
        return NegativeResponse(r, UDS_NRC_RequiredTimeDelayNotExpired);
    }

    if (!(UDSTimeAfter(UDSMillis(), srv->sec_access_auth_fail_timer))) {
        return NegativeResponse(r, UDS_NRC_ExceedNumberOfAttempts);
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
            .len = (uint16_t)(r->recv_len - UDS_0X27_REQ_BASE_LEN),
        };

        response = EmitEvent(srv, UDS_EVT_SecAccessValidateKey, &args);

        if (UDS_PositiveResponse != response) {
            srv->sec_access_auth_fail_timer =
                UDSMillis() + UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS;
            return NegativeResponse(r, response);
        }

        // "requestSeed = 0x01" identifies a fixed relationship between
        // "requestSeed = 0x01" and "sendKey = 0x02"
        // "requestSeed = 0x03" identifies a fixed relationship between
        // "requestSeed = 0x03" and "sendKey = 0x04"
        srv->securityLevel = requestedLevel;
        r->send_len = UDS_0X27_RESP_BASE_LEN;
        return UDS_PositiveResponse;
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
                .len = (uint16_t)(r->recv_len - UDS_0X27_REQ_BASE_LEN),
                .copySeed = safe_copy,
            };

            response = EmitEvent(srv, UDS_EVT_SecAccessRequestSeed, &args);

            if (UDS_PositiveResponse != response) {
                return NegativeResponse(r, response);
            }

            if (r->send_len <= UDS_0X27_RESP_BASE_LEN) { // no data was copied
                return NegativeResponse(r, UDS_NRC_GeneralProgrammingFailure);
            }
            return UDS_PositiveResponse;
        }
    }
    return NegativeResponse(r, UDS_NRC_GeneralProgrammingFailure);
}

static UDSErr_t Handle_0x28_CommunicationControl(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t controlType = r->recv_buf[1] & 0x7F;
    uint8_t communicationType = r->recv_buf[2];

    if (r->recv_len < UDS_0X28_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    UDSCommCtrlArgs_t args = {
        .ctrlType = controlType,
        .commType = communicationType,
    };

    UDSErr_t err = EmitEvent(srv, UDS_EVT_CommCtrl, &args);
    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_COMMUNICATION_CONTROL);
    r->send_buf[1] = controlType;
    r->send_len = UDS_0X28_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x2E_WriteDataByIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    uint16_t dataLen = 0;
    uint16_t dataId = 0;
    UDSErr_t err = UDS_PositiveResponse;

    /* UDS-1 2013 Figure 21 Key 1 */
    if (r->recv_len < UDS_0X2E_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    dataId = (uint16_t)((uint16_t)(r->recv_buf[1] << 8) | (uint16_t)r->recv_buf[2]);
    dataLen = (uint16_t)(r->recv_len - UDS_0X2E_REQ_BASE_LEN);

    UDSWDBIArgs_t args = {
        .dataId = dataId,
        .data = &r->recv_buf[UDS_0X2E_REQ_BASE_LEN],
        .len = dataLen,
    };

    err = EmitEvent(srv, UDS_EVT_WriteDataByIdent, &args);
    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_WRITE_DATA_BY_IDENTIFIER);
    r->send_buf[1] = dataId >> 8;
    r->send_buf[2] = dataId & 0xFF;
    r->send_len = UDS_0X2E_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x31_RoutineControl(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;
    if (r->recv_len < UDS_0X31_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t routineControlType = r->recv_buf[1] & 0x7F;
    uint16_t routineIdentifier =
        (uint16_t)((uint16_t)(r->recv_buf[2] << 8) | (uint16_t)r->recv_buf[3]);

    UDSRoutineCtrlArgs_t args = {
        .ctrlType = routineControlType,
        .id = routineIdentifier,
        .optionRecord = &r->recv_buf[UDS_0X31_REQ_MIN_LEN],
        .len = (uint16_t)(r->recv_len - UDS_0X31_REQ_MIN_LEN),
        .copyStatusRecord = safe_copy,
    };

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL);
    r->send_buf[1] = routineControlType;
    r->send_buf[2] = routineIdentifier >> 8;
    r->send_buf[3] = routineIdentifier & 0xFF;
    r->send_len = UDS_0X31_RESP_MIN_LEN;

    switch (routineControlType) {
    case UDS_LEV_RCTP_STR:  // start routine
    case UDS_LEV_RCTP_STPR: // stop routine
    case UDS_LEV_RCTP_RRR:  // request routine results
        err = EmitEvent(srv, UDS_EVT_RoutineCtrl, &args);
        if (UDS_PositiveResponse != err) {
            return NegativeResponse(r, err);
        }
        break;
    default:
        return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
    }
    return UDS_PositiveResponse;
}

static void ResetTransfer(UDSServer_t *srv) {
    UDS_ASSERT(srv);
    srv->xferBlockSequenceCounter = 1;
    srv->xferByteCounter = 0;
    srv->xferTotalBytes = 0;
    srv->xferIsActive = false;
}

static UDSErr_t Handle_0x34_RequestDownload(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;
    void *memoryAddress = 0;
    size_t memorySize = 0;

    if (srv->xferIsActive) {
        return NegativeResponse(r, UDS_NRC_ConditionsNotCorrect);
    }

    if (r->recv_len < UDS_0X34_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    err = decodeAddressAndLength(r, &r->recv_buf[2], &memoryAddress, &memorySize);
    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    UDSRequestDownloadArgs_t args = {
        .addr = memoryAddress,
        .size = memorySize,
        .dataFormatIdentifier = r->recv_buf[1],
        .maxNumberOfBlockLength = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH,
    };

    err = EmitEvent(srv, UDS_EVT_RequestDownload, &args);

    if (args.maxNumberOfBlockLength < 3) {
        UDS_LOGE(__FILE__, "maxNumberOfBlockLength too short");
        return NegativeResponse(r, UDS_NRC_GeneralProgrammingFailure);
    }

    if (UDS_PositiveResponse != err) {
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
    for (uint8_t idx = 0; idx < (uint8_t)sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = sizeof(args.maxNumberOfBlockLength) - 1 - idx;
        uint8_t byte = (args.maxNumberOfBlockLength >> (shiftBytes * 8)) & 0xFF;
        r->send_buf[UDS_0X34_RESP_BASE_LEN + idx] = byte;
    }
    r->send_len = UDS_0X34_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength);
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x35_RequestUpload(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;
    void *memoryAddress = 0;
    size_t memorySize = 0;

    if (srv->xferIsActive) {
        return NegativeResponse(r, UDS_NRC_ConditionsNotCorrect);
    }

    if (r->recv_len < UDS_0X35_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    err = decodeAddressAndLength(r, &r->recv_buf[2], &memoryAddress, &memorySize);
    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    UDSRequestUploadArgs_t args = {
        .addr = memoryAddress,
        .size = memorySize,
        .dataFormatIdentifier = r->recv_buf[1],
        .maxNumberOfBlockLength = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH,
    };

    err = EmitEvent(srv, UDS_EVT_RequestUpload, &args);

    if (args.maxNumberOfBlockLength < 3) {
        UDS_LOGE(__FILE__, "maxNumberOfBlockLength too short");
        return NegativeResponse(r, UDS_NRC_GeneralProgrammingFailure);
    }

    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    ResetTransfer(srv);
    srv->xferIsActive = true;
    srv->xferTotalBytes = memorySize;
    srv->xferBlockLength = args.maxNumberOfBlockLength;

    uint8_t lengthFormatIdentifier = sizeof(args.maxNumberOfBlockLength) << 4;

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_UPLOAD);
    r->send_buf[1] = lengthFormatIdentifier;
    for (uint8_t idx = 0; idx < (uint8_t)sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = (sizeof(args.maxNumberOfBlockLength) - 1 - idx) & 0xFF;
        uint8_t byte = (args.maxNumberOfBlockLength >> (shiftBytes * 8)) & 0xFF;
        r->send_buf[UDS_0X35_RESP_BASE_LEN + idx] = byte;
    }
    r->send_len = UDS_0X35_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength);
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x36_TransferData(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;
    uint16_t request_data_len = (uint16_t)(r->recv_len - UDS_0X36_REQ_BASE_LEN);
    uint8_t blockSequenceCounter = 0;

    if (!srv->xferIsActive) {
        return NegativeResponse(r, UDS_NRC_UploadDownloadNotAccepted);
    }

    if (r->recv_len < UDS_0X36_REQ_BASE_LEN) {
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    blockSequenceCounter = r->recv_buf[1];

    if (!srv->RCRRP) {
        if (blockSequenceCounter != srv->xferBlockSequenceCounter) {
            err = UDS_NRC_RequestSequenceError;
            goto fail;
        } else {
            srv->xferBlockSequenceCounter++;
        }
    }

    if (srv->xferByteCounter + request_data_len > srv->xferTotalBytes) {
        err = UDS_NRC_TransferDataSuspended;
        goto fail;
    }

    {
        UDSTransferDataArgs_t args = {
            .data = &r->recv_buf[UDS_0X36_REQ_BASE_LEN],
            .len = (uint16_t)(r->recv_len - UDS_0X36_REQ_BASE_LEN),
            .maxRespLen = (uint16_t)(srv->xferBlockLength - UDS_0X36_RESP_BASE_LEN),
            .copyResponse = safe_copy,
        };

        r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TRANSFER_DATA);
        r->send_buf[1] = blockSequenceCounter;
        r->send_len = UDS_0X36_RESP_BASE_LEN;

        err = EmitEvent(srv, UDS_EVT_TransferData, &args);

        if (err == UDS_PositiveResponse) {
            srv->xferByteCounter += request_data_len;
            return UDS_PositiveResponse;
        } else if (err == UDS_NRC_RequestCorrectlyReceived_ResponsePending) {
            return NegativeResponse(r, UDS_NRC_RequestCorrectlyReceived_ResponsePending);
        } else {
            goto fail;
        }
    }

fail:
    ResetTransfer(srv);
    return NegativeResponse(r, err);
}

static UDSErr_t Handle_0x37_RequestTransferExit(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;

    if (!srv->xferIsActive) {
        return NegativeResponse(r, UDS_NRC_UploadDownloadNotAccepted);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_TRANSFER_EXIT);
    r->send_len = UDS_0X37_RESP_BASE_LEN;

    UDSRequestTransferExitArgs_t args = {
        .data = &r->recv_buf[UDS_0X37_REQ_BASE_LEN],
        .len = (uint16_t)(r->recv_len - UDS_0X37_REQ_BASE_LEN),
        .copyResponse = safe_copy,
    };

    err = EmitEvent(srv, UDS_EVT_RequestTransferExit, &args);

    if (err == UDS_PositiveResponse) {
        ResetTransfer(srv);
        return UDS_PositiveResponse;
    } else if (err == UDS_NRC_RequestCorrectlyReceived_ResponsePending) {
        return NegativeResponse(r, UDS_NRC_RequestCorrectlyReceived_ResponsePending);
    } else {
        ResetTransfer(srv);
        return NegativeResponse(r, err);
    }
}

static UDSErr_t Handle_0x38_RequestFileTransfer(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;

    if (srv->xferIsActive) {
        return NegativeResponse(r, UDS_NRC_ConditionsNotCorrect);
    }
    if (r->recv_len < UDS_0X38_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t mode_of_operation = r->recv_buf[1];
    if (mode_of_operation < UDS_MOOP_ADDFILE || mode_of_operation > UDS_MOOP_RDFILE) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }
    uint16_t file_path_len = (uint16_t)(((uint16_t)r->recv_buf[2] << 8) + (uint16_t)r->recv_buf[3]);
    uint8_t data_format_identifier = 0;
    uint8_t file_size_parameter_length = 0;
    size_t file_size_uncompressed = 0;
    size_t file_size_compressed = 0;
    uint16_t byte_idx = 4 + file_path_len;

    if (byte_idx > r->recv_len) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    if ((mode_of_operation == UDS_MOOP_DELFILE) || (mode_of_operation == UDS_MOOP_RDDIR)) {
        // ISO14229:2020 Table 481:
        // If the modeOfOperation parameter equals to 0x02 (DeleteFile) and 0x05 (ReadDir) this
        // parameter [dataFormatIdentifier] shall not be included in the request message.
    } else {
        data_format_identifier = r->recv_buf[byte_idx];
        byte_idx++;
    }

    if ((mode_of_operation == UDS_MOOP_DELFILE) || (mode_of_operation == UDS_MOOP_RDFILE) ||
        (mode_of_operation == UDS_MOOP_RDDIR)) {
        // ISO14229:2020 Table 481:
        // If the modeOfOperation parameter equals to 0x02 (DeleteFile), 0x04 (ReadFile) or 0x05
        // (ReadDir) this parameter [fileSizeParameterLength] shall not be included in the request
        // message. If the modeOfOperation parameter equals to 0x02 (DeleteFile), 0x04 (ReadFile) or
        // 0x05 (ReadDir) this parameter [fileSizeUncompressed] shall not be included in the request
        // message. If the modeOfOperation parameter equals to 0x02 (DeleteFile), 0x04 (ReadFile) or
        // 0x05 (ReadDir) this parameter [fileSizeCompressed] shall not be included in the request
        // message.
    } else {
        file_size_parameter_length = r->recv_buf[byte_idx];
        byte_idx++;

        static_assert(sizeof(file_size_uncompressed) == sizeof(file_size_compressed),
                      "Both should be k-byte numbers per Table 480");
        if (file_size_parameter_length > sizeof(file_size_compressed)) {
            return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
        }
        if (byte_idx + 2 * file_size_uncompressed > r->recv_len) {
            return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
        }
        for (size_t i = 0; i < file_size_parameter_length; i++) {
            uint8_t data_byte = r->recv_buf[byte_idx];
            uint8_t shift_by_bytes = file_size_parameter_length - i - 1;
            file_size_uncompressed |= (size_t)data_byte << (8 * shift_by_bytes);
            byte_idx++;
        }
        for (size_t i = 0; i < file_size_parameter_length; i++) {
            uint8_t data_byte = r->recv_buf[byte_idx];
            uint8_t shift_by_bytes = file_size_parameter_length - i - 1;
            file_size_compressed |= (size_t)data_byte << (8 * shift_by_bytes);
            byte_idx++;
        }
    }

    UDSRequestFileTransferArgs_t args = {
        .modeOfOperation = mode_of_operation,
        .filePathLen = file_path_len,
        .filePath = &r->recv_buf[4],
        .dataFormatIdentifier = data_format_identifier,
        .fileSizeUnCompressed = file_size_uncompressed,
        .fileSizeCompressed = file_size_compressed,
    };

    err = EmitEvent(srv, UDS_EVT_RequestFileTransfer, &args);

    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    ResetTransfer(srv);
    srv->xferIsActive = true;
    srv->xferTotalBytes = args.fileSizeCompressed;
    srv->xferBlockLength = args.maxNumberOfBlockLength;

    if (args.maxNumberOfBlockLength > UDS_TP_MTU) {
        args.maxNumberOfBlockLength = UDS_TP_MTU;
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_FILE_TRANSFER);
    r->send_buf[1] = args.modeOfOperation;
    r->send_buf[2] = sizeof(args.maxNumberOfBlockLength);
    for (uint8_t idx = 0; idx < (uint8_t)sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = sizeof(args.maxNumberOfBlockLength) - 1 - idx;
        uint8_t byte = (uint8_t)(args.maxNumberOfBlockLength >> (shiftBytes * 8));
        r->send_buf[UDS_0X38_RESP_BASE_LEN + idx] = byte;
    }
    r->send_buf[UDS_0X38_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength)] =
        args.dataFormatIdentifier;

    r->send_len = UDS_0X38_RESP_BASE_LEN + sizeof(args.maxNumberOfBlockLength) + 1;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x3D_WriteMemoryByAddress(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    void *address = 0;
    size_t length = 0;

    if (r->recv_len < UDS_0X3D_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    ret = decodeAddressAndLength(r, &r->recv_buf[1], &address, &length);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    uint8_t memorySizeLength = (r->recv_buf[1] & 0xF0) >> 4;
    uint8_t memoryAddressLength = r->recv_buf[1] & 0x0F;

    uint8_t dataOffset = 2 + memorySizeLength + memoryAddressLength;

    if (dataOffset + length != r->recv_len) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    UDSWriteMemByAddrArgs_t args = {
        .memAddr = address,
        .memSize = length,
        .data = &r->recv_buf[dataOffset],
    };

    ret = EmitEvent(srv, UDS_EVT_WriteMemByAddr, &args);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_WRITE_MEMORY_BY_ADDRESS);
    // echo addressAndLengthFormatIdentifier, memoryAddress, and memorySize
    memcpy(&r->send_buf[1], &r->recv_buf[1], 1 + memorySizeLength + memoryAddressLength);
    r->send_len = UDS_0X3D_RESP_BASE_LEN + memorySizeLength + memoryAddressLength;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x3E_TesterPresent(UDSServer_t *srv, UDSReq_t *r) {
    if ((r->recv_len < UDS_0X3E_REQ_MIN_LEN) || (r->recv_len > UDS_0X3E_REQ_MAX_LEN)) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }
    uint8_t zeroSubFunction = r->recv_buf[1];

    switch (zeroSubFunction) {
    case 0x00:
    case 0x80:
        srv->s3_session_timeout_timer = UDSMillis() + srv->s3_ms;
        r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TESTER_PRESENT);
        r->send_buf[1] = 0x00;
        r->send_len = UDS_0X3E_RESP_LEN;
        return UDS_PositiveResponse;
    default:
        return NegativeResponse(r, UDS_NRC_SubFunctionNotSupported);
    }
}

static UDSErr_t Handle_0x85_ControlDTCSetting(UDSServer_t *srv, UDSReq_t *r) {
    (void)srv;
    if (r->recv_len < UDS_0X85_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }
    uint8_t dtcSettingType = r->recv_buf[1] & 0x3F;

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_CONTROL_DTC_SETTING);
    r->send_buf[1] = dtcSettingType;
    r->send_len = UDS_0X85_RESP_LEN;
    return UDS_PositiveResponse;
}

typedef UDSErr_t (*UDSService)(UDSServer_t *srv, UDSReq_t *r);

/**
 * @brief Get the internal service handler matching the given SID.
 * @param sid
 * @return pointer to UDSService or NULL if no match
 */
static UDSService getServiceForSID(uint8_t sid) {
    switch (sid) {
    case kSID_DIAGNOSTIC_SESSION_CONTROL:
        return Handle_0x10_DiagnosticSessionControl;
    case kSID_ECU_RESET:
        return Handle_0x11_ECUReset;
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
        return NULL;
    case kSID_READ_DTC_INFORMATION:
        return Handle_0x19_ReadDTCInformation;
    case kSID_READ_DATA_BY_IDENTIFIER:
        return Handle_0x22_ReadDataByIdentifier;
    case kSID_READ_MEMORY_BY_ADDRESS:
        return Handle_0x23_ReadMemoryByAddress;
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
        return NULL;
    case kSID_SECURITY_ACCESS:
        return Handle_0x27_SecurityAccess;
    case kSID_COMMUNICATION_CONTROL:
        return Handle_0x28_CommunicationControl;
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
        return NULL;
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
        return NULL;
    case kSID_WRITE_DATA_BY_IDENTIFIER:
        return Handle_0x2E_WriteDataByIdentifier;
    case kSID_INPUT_CONTROL_BY_IDENTIFIER:
        return NULL;
    case kSID_ROUTINE_CONTROL:
        return Handle_0x31_RoutineControl;
    case kSID_REQUEST_DOWNLOAD:
        return Handle_0x34_RequestDownload;
    case kSID_REQUEST_UPLOAD:
        return Handle_0x35_RequestUpload;
    case kSID_TRANSFER_DATA:
        return Handle_0x36_TransferData;
    case kSID_REQUEST_TRANSFER_EXIT:
        return Handle_0x37_RequestTransferExit;
    case kSID_REQUEST_FILE_TRANSFER:
        return Handle_0x38_RequestFileTransfer;
    case kSID_WRITE_MEMORY_BY_ADDRESS:
        return Handle_0x3D_WriteMemoryByAddress;
    case kSID_TESTER_PRESENT:
        return Handle_0x3E_TesterPresent;
    case kSID_ACCESS_TIMING_PARAMETER:
        return NULL;
    case kSID_SECURED_DATA_TRANSMISSION:
        return NULL;
    case kSID_CONTROL_DTC_SETTING:
        return Handle_0x85_ControlDTCSetting;
    case kSID_RESPONSE_ON_EVENT:
        return NULL;
    default:
        UDS_LOGI(__FILE__, "no handler for request SID %x", sid);
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
static UDSErr_t evaluateServiceResponse(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t response = UDS_PositiveResponse;
    bool suppressResponse = false;
    uint8_t sid = r->recv_buf[0];
    UDSService service = getServiceForSID(sid);

    if (NULL == srv->fn)
        return NegativeResponse(r, UDS_NRC_ServiceNotSupported);
    UDS_ASSERT(srv->fn); // service handler functions will call srv->fn. it must be valid

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
        assert(service);
        response = service(srv, r);

        bool suppressPosRspMsgIndicationBit = r->recv_buf[1] & 0x80;

        /* test if positive response is required and if responseCode is positive 0x00 */
        if ((suppressPosRspMsgIndicationBit) && (response == UDS_PositiveResponse) &&
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
    case kSID_REQUEST_FILE_TRANSFER:
    case kSID_REQUEST_TRANSFER_EXIT: {
        assert(service);
        response = service(srv, r);
        break;
    }

    /* CASE Service_optional */
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
    case kSID_READ_DTC_INFORMATION:
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
    case kSID_INPUT_CONTROL_BY_IDENTIFIER:
    case kSID_WRITE_MEMORY_BY_ADDRESS:
    case kSID_ACCESS_TIMING_PARAMETER:
    case kSID_SECURED_DATA_TRANSMISSION:
    case kSID_RESPONSE_ON_EVENT:
    default: {
        if (service) {
            response = service(srv, r);
        } else { /* getServiceForSID(sid) returned NULL*/
            UDSCustomArgs_t args = {
                .sid = sid,
                .optionRecord = &r->recv_buf[1],
                .len = (uint16_t)(r->recv_len - 1),
                .copyResponse = safe_copy,
            };

            r->send_buf[0] = UDS_RESPONSE_SID_OF(sid);
            r->send_len = 1;

            response = EmitEvent(srv, UDS_EVT_Custom, &args);
            if (UDS_PositiveResponse != response)
                return NegativeResponse(r, response);
        }
        break;
    }
    }

    if ((UDS_A_TA_TYPE_FUNCTIONAL == r->info.A_TA_Type) &&
        ((UDS_NRC_ServiceNotSupported == response) ||
         (UDS_NRC_SubFunctionNotSupported == response) ||
         (UDS_NRC_ServiceNotSupportedInActiveSession == response) ||
         (UDS_NRC_SubFunctionNotSupportedInActiveSession == response) ||
         (UDS_NRC_RequestOutOfRange == response)) &&
        (
            // TODO: *not yet a NRC 0x78 response sent*
            true)) {
        /* Suppress negative response message */
        suppressResponse = true;
    }

    if (suppressResponse) {
        NoResponse(r);
    } else { /* send negative or positive response */
        ;
    }

    return response;
}

// ========================================================================
//                             Public Functions
// ========================================================================

UDSErr_t UDSServerInit(UDSServer_t *srv) {
    if (NULL == srv) {
        return UDS_ERR_INVALID_ARG;
    }
    memset(srv, 0, sizeof(UDSServer_t));
    srv->p2_ms = UDS_SERVER_DEFAULT_P2_MS;
    srv->p2_star_ms = UDS_SERVER_DEFAULT_P2_STAR_MS;
    srv->s3_ms = UDS_SERVER_DEFAULT_S3_MS;
    srv->sessionType = UDS_LEV_DS_DS;
    srv->p2_timer = UDSMillis() + srv->p2_ms;
    srv->s3_session_timeout_timer = UDSMillis() + srv->s3_ms;
    srv->sec_access_boot_delay_timer =
        UDSMillis() + UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS;
    srv->sec_access_auth_fail_timer = UDSMillis();
    return UDS_OK;
}

void UDSServerPoll(UDSServer_t *srv) {
    // UDS-1-2013 Figure 38: Session Timeout (S3)
    if (UDS_LEV_DS_DS != srv->sessionType &&
        UDSTimeAfter(UDSMillis(), srv->s3_session_timeout_timer)) {
        EmitEvent(srv, UDS_EVT_SessionTimeout, NULL);
        srv->sessionType = UDS_LEV_DS_DS;
        srv->securityLevel = 0;
    }

    if (srv->ecuResetScheduled && UDSTimeAfter(UDSMillis(), srv->ecuResetTimer)) {
        EmitEvent(srv, UDS_EVT_DoScheduledReset, &srv->ecuResetScheduled);
    }

    UDSTpPoll(srv->tp);

    UDSReq_t *r = &srv->r;

    if (srv->requestInProgress) {
        if (srv->RCRRP) {
            // responds only if
            // 1. changed (no longer RCRRP), or
            // 2. p2_timer has elapsed
            UDSErr_t response = evaluateServiceResponse(srv, r);
            if (UDS_NRC_RequestCorrectlyReceived_ResponsePending == response) {
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
            ssize_t ret = 0;
            if (r->send_len) {
                ret = UDSTpSend(srv->tp, r->send_buf, r->send_len, NULL);
            }

            // TODO test injection of transport errors:
            if (ret < 0) {
                UDSErr_t err = UDS_ERR_TPORT;
                EmitEvent(srv, UDS_EVT_Err, &err);
                UDS_LOGE(__FILE__, "UDSTpSend failed with %zd\n", ret);
            }

            if (srv->RCRRP) {
                // ISO14229-2:2013 Table 4 footnote b
                // min time between consecutive 0x78 responses is 0.3 * p2*
                uint32_t wait_time = srv->p2_star_ms * 3 / 10;
                srv->p2_timer = UDSMillis() + wait_time;
            } else {
                srv->p2_timer = UDSMillis() + srv->p2_ms;
                srv->requestInProgress = false;
            }
        }

    } else {
        if (srv->notReadyToReceive) {
            return; // cannot respond to request right now
        }
        ssize_t len = UDSTpRecv(srv->tp, r->recv_buf, sizeof(r->recv_buf), &r->info);
        if (len < 0) {
            UDS_LOGE(__FILE__, "UDSTpRecv failed with %zd\n", r->recv_len);
            return;
        }

        r->recv_len = (size_t)len;

        if (r->recv_len > 0) {
            UDSErr_t response = evaluateServiceResponse(srv, r);
            srv->requestInProgress = true;
            if (UDS_NRC_RequestCorrectlyReceived_ResponsePending == response) {
                srv->RCRRP = true;
            }
        }
    }
}
