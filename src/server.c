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

    UDSErr_t err = UDS_OK;

    // when transitioning from a non-default session to a default session
    if (srv->sessionType != UDS_LEV_DS_DS && args.type == UDS_LEV_DS_DS) {
        // ignore event returncode as we don't force user to handle this event
        EmitEvent(srv, UDS_EVT_AuthTimeout, NULL);
    }

    err = EmitEvent(srv, UDS_EVT_DiagSessCtrl, &args);

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

static UDSErr_t Handle_0x14_ClearDiagnosticInformation(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X14_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_CLEAR_DIAGNOSTIC_INFORMATION);
    r->send_len = UDS_0X14_RESP_BASE_LEN;

    UDSCDIArgs_t args = {
        .groupOfDTC = (uint32_t)((r->recv_buf[1] << 16) | (r->recv_buf[2] << 8) | r->recv_buf[3]),
        .hasMemorySelection = (r->recv_len >= 5),
        .memorySelection = (r->recv_len >= 5) ? r->recv_buf[4] : 0,
    };

    UDSErr_t err = EmitEvent(srv, UDS_EVT_ClearDiagnosticInfo, &args);

    if (err != UDS_PositiveResponse) {
        return NegativeResponse(r, err);
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

    /* Shared by all SubFunc */
    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_DTC_INFORMATION);
    r->send_buf[1] = type;
    r->send_len = UDS_0X19_RESP_BASE_LEN;

    UDSRDTCIArgs_t args = {
        .type = type,
        .copy = safe_copy,
    };

    /* Before checks and emitting Request */
    switch (type) {
    case 0x01: /* reportNumberOfDTCByStatusMask */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 1) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.numOfDTCByStatusMaskArgs.mask = r->recv_buf[2];
        break;
    case 0x02: /* reportDTCByStatusMask */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 1) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcStatusByMaskArgs.mask = r->recv_buf[2];
        break;
    case 0x03: /* reportDTCSnapshotIdentification */
    case 0x0A: /* reportSupportedDTC */
    case 0x0B: /* reportFirstTestFailedDTC */
    case 0x0C: /* reportFirstConfirmedDTC */
    case 0x0D: /* reportMostRecentTestFailedDTC */
    case 0x0E: /* reportMostRecentConfirmedDTC */
    case 0x14: /* reportDTCFaultDetectionCounter */
    case 0x15: /* reportDTCWithPermanentStatus */
        /* has no subfunction specific args */
        break;
    case 0x04: /* reportDTCSnapshotRecordByDTCNumber */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 4) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcSnapshotRecordbyDTCNumArgs.dtc =
            (r->recv_buf[2] << 16 | r->recv_buf[3] << 8 | r->recv_buf[4]) & 0x00FFFFFF;
        args.subFuncArgs.dtcSnapshotRecordbyDTCNumArgs.snapshotNum = r->recv_buf[5];
        break;
    case 0x05: /* reportDTCStoredDataByRecordNumber */
    case 0x16: /* reportDTCExtDataRecordByNumber */
    case 0x1A: /* reportDTCExtendedDataRecordIdentification */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 1) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcStoredDataByRecordNumArgs.recordNum = r->recv_buf[2];
        break;
    case 0x06: /* reportDTCExtDataRecordByDTCNumber */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 4) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcExtDtaRecordByDTCNumArgs.dtc =
            (r->recv_buf[2] << 16 | r->recv_buf[3] << 8 | r->recv_buf[4]) & 0x00FFFFFF;
        args.subFuncArgs.dtcExtDtaRecordByDTCNumArgs.extDataRecNum = r->recv_buf[5];
        break;
    case 0x07: /* reportNumberOfDTCBySeverityMaskRecord */
    case 0x08: /* reportDTCBySeverityMaskRecord */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 2) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.numOfDTCBySeverityMaskArgs.severityMask = r->recv_buf[2];
        args.subFuncArgs.numOfDTCBySeverityMaskArgs.statusMask = r->recv_buf[3];
        break;
    case 0x09: /* reportSeverityInformationOfDTC */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 1) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.severityInfoOfDTCArgs.dtc =
            (r->recv_buf[2] << 16 | r->recv_buf[3] << 8 | r->recv_buf[4]) & 0x00FFFFFF;
        break;
    case 0x17: /* reportUserDefMemoryDTCByStatusMask */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 2) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.userDefMemoryDTCByStatusMaskArgs.mask = r->recv_buf[2];
        args.subFuncArgs.userDefMemoryDTCByStatusMaskArgs.memory = r->recv_buf[3];
        break;
    case 0x18: /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 5) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.dtc =
            (r->recv_buf[2] << 16 | r->recv_buf[3] << 8 | r->recv_buf[4]) & 0x00FFFFFF;
        args.subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.snapshotNum = r->recv_buf[5];
        args.subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.memory = r->recv_buf[6];
        break;
    case 0x19: /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 5) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.dtc =
            (r->recv_buf[2] << 16 | r->recv_buf[3] << 8 | r->recv_buf[4]) & 0x00FFFFFF;
        args.subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.extDataRecNum = r->recv_buf[5];
        args.subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.memory = r->recv_buf[6];
        break;
    case 0x42: /* reportWWHOBDDTCByMaskRecord */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 3) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.wwhobdDTCByMaskArgs.functionalGroup = r->recv_buf[2];
        args.subFuncArgs.wwhobdDTCByMaskArgs.statusMask = r->recv_buf[3];
        args.subFuncArgs.wwhobdDTCByMaskArgs.severityMask = r->recv_buf[4];
        break;
    case 0x55: /* reportWWHOBDDTCWithPermanentStatus */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 1) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.wwhobdDTCWithPermStatusArgs.functionalGroup = r->recv_buf[2];
        break;
    case 0x56: /* reportDTCInformationByDTCReadinessGroupIdentifier */
        if (r->recv_len < UDS_0X19_REQ_MIN_LEN + 2) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcInfoByDTCReadinessGroupIdArgs.functionalGroup = r->recv_buf[2];
        args.subFuncArgs.dtcInfoByDTCReadinessGroupIdArgs.readinessGroup = r->recv_buf[3];
        break;
    default:
        return NegativeResponse(r, UDS_NRC_SubFunctionNotSupported);
    }

    ret = EmitEvent(srv, UDS_EVT_ReadDTCInformation, &args);

    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    if (r->send_len < UDS_0X19_RESP_BASE_LEN) {
        goto respond_to_0x19_malformed_response;
    }

    /* subfunc specific reply len checks */
    switch (type) {
    case 0x01: /* reportNumberOfDTCByStatusMask */
    case 0x07: /* reportNumberOfDTCBySeverityMaskRecord */
        if (r->send_len != UDS_0X19_RESP_BASE_LEN + 4) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x02: /* reportDTCByStatusMask */
    case 0x0A: /* reportSupportedDTC */
    case 0x0B: /* reportFirstTestFailedDTC */
    case 0x0C: /* reportFirstConfirmedDTC */
    case 0x0D: /* reportMostRecentTestFailedDTC */
    case 0x0E: /* reportMostRecentConfirmedDTC */
    case 0x15: /* reportDTCWithPermanentStatus */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 1 ||
            ((r->send_len > UDS_0X19_RESP_BASE_LEN + 1) &&
             (r->send_len - (UDS_0X19_RESP_BASE_LEN + 1)) % 4 != 0)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x03: /* reportDTCSnapshotIdentification */
    case 0x14: /* reportDTCFaultDetectionCounter */
        if ((r->send_len - UDS_0X19_RESP_BASE_LEN) % 4 != 0) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x04: /* reportDTCSnapshotRecordByDTCNumber */
    case 0x06: /* reportDTCExtDataRecordByDTCNumber */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 4) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x05: /* reportDTCStoredDataByRecordNumber */
    case 0x16: /* reportDTCExtDataRecordByNumber */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 1) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x08: /* reportDTCBySeverityMaskRecord */
    case 0x09: /* reportSeverityInformationOfDTC */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 1 ||
            ((r->send_len > UDS_0X19_RESP_BASE_LEN + 1) &&
             (r->send_len - (UDS_0X19_RESP_BASE_LEN + 1)) % 6 != 0)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x17: /* reportUserDefMemoryDTCByStatusMask */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 2 ||
            ((r->send_len > UDS_0X19_RESP_BASE_LEN + 2) &&
             (r->send_len - (UDS_0X19_RESP_BASE_LEN + 2)) % 4 != 0)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x18: /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
    case 0x19: /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 5) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x1A: /* reportDTCExtendedDataRecordIdentification */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 1 ||
            ((r->send_len != UDS_0X19_RESP_BASE_LEN + 6) &&
             (r->send_len > UDS_0X19_RESP_BASE_LEN + 1) &&
             (r->send_len < UDS_0X19_RESP_BASE_LEN + 4)) ||
            ((r->send_len > UDS_0X19_RESP_BASE_LEN + 6) &&
             (r->send_len - UDS_0X19_RESP_BASE_LEN + 6) % 4 != 0)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x42: /* reportWWHOBDDTCByMaskRecord */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 4 ||
            ((r->send_len > UDS_0X19_RESP_BASE_LEN + 4) &&
             (r->send_len - (UDS_0X19_RESP_BASE_LEN + 4)) % 5 != 0)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x55: /* reportWWHOBDDTCWithPermanentStatus */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 3 ||
            ((r->send_len > UDS_0X19_RESP_BASE_LEN + 3) &&
             (r->send_len - (UDS_0X19_RESP_BASE_LEN + 3)) % 4 != 0)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x56: /* reportDTCInformationByDTCReadinessGroupIdentifier */
        if (r->send_len < UDS_0X19_RESP_BASE_LEN + 4 ||
            ((r->send_len > UDS_0X19_RESP_BASE_LEN + 4) &&
             (r->send_len - (UDS_0X19_RESP_BASE_LEN + 4)) % 4 != 0)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    default:
        UDS_LOGW(__FILE__, "RDTCI subFunc 0x%02X is not supported.\n", type);
        return NegativeResponse(r, UDS_NRC_SubFunctionNotSupported);
    }

    return UDS_PositiveResponse;
respond_to_0x19_malformed_response:
    UDS_LOGE(__FILE__, "RDTCI subFunc 0x%02X is malformed. Length: %zu\n", type, r->send_len);
    return NegativeResponse(r, UDS_NRC_GeneralReject);
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

        size_t send_len_before = r->send_len;
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
 * @brief decode the addressAndLengthFormatIdentifier that appears in
 * DynamicallyDefineDataIdentifier (0x2C). This must be handled separatedly because the
 * format identifier is not directly above the memory address and length.
 *
 * @param srv
 * @param buf pointer to addressAndDataLengthFormatIdentifier in recv_buf
 * @param memoryAddress the decoded memory address
 * @param memorySize the decoded memory size
 * @param offset how many elements (addres and size pairs) away from the format identifier
 * @return uint8_t
 */
static UDSErr_t decodeAddressAndLengthWithOffset(UDSReq_t *r, uint8_t *const buf,
                                                 void **memoryAddress, size_t *memorySize,
                                                 size_t offset) {
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
    size_t offsetBytes = offset * (memoryAddressLength + memorySizeLength);

    if (memorySizeLength == 0 || memorySizeLength > sizeof(size_t)) {
        return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
    }

    if (memoryAddressLength == 0 || memoryAddressLength > sizeof(size_t)) {
        return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
    }

    if (buf + 1 + offsetBytes + memorySizeLength + memoryAddressLength >
        r->recv_buf + r->recv_len) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    for (int byteIdx = 0; byteIdx < memoryAddressLength; byteIdx++) {
        long long unsigned int byte = buf[1 + offsetBytes + byteIdx];
        uint8_t shiftBytes = (uint8_t)(memoryAddressLength - 1 - byteIdx);
        tmp |= byte << (8 * shiftBytes);
    }
    *memoryAddress = (void *)tmp;

    for (int byteIdx = 0; byteIdx < memorySizeLength; byteIdx++) {
        uint8_t byte = buf[1 + offsetBytes + memoryAddressLength + byteIdx];
        uint8_t shiftBytes = (uint8_t)(memorySizeLength - 1 - byteIdx);
        *memorySize |= (size_t)byte << (8 * shiftBytes);
    }
    return UDS_PositiveResponse;
}

/**
 * @brief decode the addressAndLengthFormatIdentifier that appears in ReadMemoryByAddress (0x23)
 * and RequestDownload (0X34)
 *
 * @param srv
 * @param buf pointer to addressAndDataLengthFormatIdentifier in recv_buf
 * @param memoryAddress the decoded memory address
 * @param memorySize the decoded memory size
 * @return uint8_t
 */
static UDSErr_t decodeAddressAndLength(UDSReq_t *r, uint8_t *const buf, void **memoryAddress,
                                       size_t *memorySize) {
    return decodeAddressAndLengthWithOffset(r, buf, memoryAddress, memorySize, 0);
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
        UDS_LOGE(__FILE__, "response positive but not all data sent: expected %zu, sent %zu",
                 length, r->send_len - UDS_0X23_RESP_BASE_LEN);
        return NegativeResponse(r, UDS_NRC_GeneralReject);
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
                UDS_LOGE(__FILE__, "0x27: no seed data was copied");
                return NegativeResponse(r, UDS_NRC_GeneralReject);
            }
            return UDS_PositiveResponse;
        }
    }
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
        .nodeId = 0,
    };

    if (args.ctrlType == 0x04 || args.ctrlType == 0x05) {
        if (r->recv_len < UDS_0X28_REQ_BASE_LEN + 2) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }
        args.nodeId = (uint16_t)((uint16_t)(r->recv_buf[3] << 8) | (uint16_t)r->recv_buf[4]);
    }

    UDSErr_t err = EmitEvent(srv, UDS_EVT_CommCtrl, &args);
    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_COMMUNICATION_CONTROL);
    r->send_buf[1] = controlType;
    r->send_len = UDS_0X28_RESP_LEN;
    return UDS_PositiveResponse;
}

static uint8_t set_auth_state(UDSServer_t *srv, uint8_t state) {
    if (srv == NULL) {
        return UDS_NRC_GeneralReject;
    }
    UDSReq_t *r = (UDSReq_t *)&srv->r;
    r->send_buf[2] = state;

    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x29_Authentication(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    uint8_t type = r->recv_buf[1];

    if (r->recv_len < UDS_0X29_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_AUTHENTICATION);
    r->send_buf[1] = type;
    r->send_buf[2] = UDS_AT_GR; /* Expect this to be overridden by the user event handler */
    r->send_len = UDS_0X29_RESP_BASE_LEN;

    UDSAuthArgs_t args = {
        .type = type,
        .set_auth_state = set_auth_state,
        .copy = safe_copy,
    };

    switch (type) {
    case UDS_LEV_AT_DA:
    case UDS_LEV_AT_AC:
        /* No custom check necessary */
        break;
    case UDS_LEV_AT_VCU:
    case UDS_LEV_AT_VCB: {
        /**
         * + 1 byte communication configuration
         * + 2 bytes length of certificate
         * + 0 bytes certificate
         * + 2 bytes length of challenge
         * + 0 bytes challenge
         */
        size_t min_recv_len = UDS_0X29_REQ_MIN_LEN + 1 + 2 + 2;

        if (r->recv_len < min_recv_len) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.verifyCertArgs.commConf = r->recv_buf[2];
        args.subFuncArgs.verifyCertArgs.certLen =
            (uint16_t)((uint16_t)(r->recv_buf[3] << 8) | (uint16_t)r->recv_buf[4]);
        args.subFuncArgs.verifyCertArgs.cert = &r->recv_buf[5];

        if (args.subFuncArgs.verifyCertArgs.certLen == 0) {
            UDS_LOGW(__FILE__, "Auth: VCU/B verify certificate with zero length\n");
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        if (args.subFuncArgs.verifyCertArgs.certLen > r->recv_len - min_recv_len) {
            UDS_LOGW(__FILE__, "Auth: VCU/B verify certificate too large: %u",
                     args.subFuncArgs.verifyCertArgs.certLen);
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.verifyCertArgs.challengeLen =
            (uint16_t)((uint16_t)(r->recv_buf[5 + args.subFuncArgs.verifyCertArgs.certLen] << 8) |
                       (uint16_t)r->recv_buf[6 + args.subFuncArgs.verifyCertArgs.certLen]);
        args.subFuncArgs.verifyCertArgs.challenge =
            &r->recv_buf[7 + args.subFuncArgs.verifyCertArgs.certLen];

        if (type == UDS_LEV_AT_VCB && args.subFuncArgs.verifyCertArgs.challengeLen == 0) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        if (r->recv_len != min_recv_len + args.subFuncArgs.verifyCertArgs.certLen +
                               args.subFuncArgs.verifyCertArgs.challengeLen) {
            UDS_LOGW(__FILE__,
                     "Auth: VCU/B request malformed length. req len: %u, cert len: %u, "
                     "challenge len: %u\n",
                     r->recv_len, args.subFuncArgs.verifyCertArgs.certLen,
                     args.subFuncArgs.verifyCertArgs.challengeLen);
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        break;
    }
    case UDS_LEV_AT_POWN: {
        /**
         * + 2 bytes length of pown
         * + 0 bytes pown
         * + 2 bytes length of ephemeral public key
         * + 0 bytes ephemeral public key
         */
        size_t min_recv_len = UDS_0X29_REQ_MIN_LEN + 2 + 2;

        if (r->recv_len < min_recv_len) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.pownArgs.pownLen =
            (uint16_t)((uint16_t)(r->recv_buf[2] << 8) | (uint16_t)r->recv_buf[3]);
        args.subFuncArgs.pownArgs.pown = &r->recv_buf[4];

        if (args.subFuncArgs.pownArgs.pownLen == 0) {
            UDS_LOGW(__FILE__, "Auth: POWN with zero length\n");
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.pownArgs.publicKeyLen =
            (uint16_t)((uint16_t)(r->recv_buf[4 + args.subFuncArgs.pownArgs.pownLen] << 8) |
                       (uint16_t)r->recv_buf[5 + args.subFuncArgs.pownArgs.pownLen]);
        args.subFuncArgs.pownArgs.publicKey = &r->recv_buf[6 + args.subFuncArgs.pownArgs.pownLen];

        if (r->recv_len != min_recv_len + args.subFuncArgs.pownArgs.pownLen +
                               args.subFuncArgs.pownArgs.publicKeyLen) {
            UDS_LOGW(__FILE__,
                     "Auth: POWN request malformed length. req len: %u, pown len: %u, "
                     "public key len: %u\n",
                     r->recv_len, args.subFuncArgs.pownArgs.pownLen,
                     args.subFuncArgs.pownArgs.publicKeyLen);
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        break;
    }

    case UDS_LEV_AT_TC: {
        /**
         * + 1 byte for evaluation ID
         * + 2 bytes length of certificate
         */
        size_t min_recv_len = UDS_0X29_REQ_MIN_LEN + 1 + 2;

        if (r->recv_len < min_recv_len) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.transCertArgs.evalId = r->recv_buf[2];
        args.subFuncArgs.transCertArgs.len =
            (uint16_t)((uint16_t)(r->recv_buf[3] << 8) | (uint16_t)r->recv_buf[4]);
        args.subFuncArgs.transCertArgs.cert = &r->recv_buf[5];
        if (args.subFuncArgs.transCertArgs.len == 0) {
            UDS_LOGW(__FILE__, "Auth: TC with zero certificate length\n");
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }
        if (r->recv_len != min_recv_len + args.subFuncArgs.transCertArgs.len) {
            UDS_LOGW(__FILE__, "Auth: TC request malformed length. req len: %u, cert len: %u\n",
                     r->recv_len, args.subFuncArgs.transCertArgs.len);
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        break;
    }
    case UDS_LEV_AT_RCFA:
        /**
         * + 1 byte for communication configuration
         * + 16 bytes for algorithm ID
         */
        if (r->recv_len < UDS_0X29_REQ_MIN_LEN + 1 + 16) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.reqChallengeArgs.commConf = r->recv_buf[2];
        args.subFuncArgs.reqChallengeArgs.algoInd = &r->recv_buf[3];

        memcpy(&r->send_buf[3], args.subFuncArgs.reqChallengeArgs.algoInd, 16);
        r->send_len += 16;
        break;
    case UDS_LEV_AT_VPOWNU:
    case UDS_LEV_AT_VPOWNB:
        /**
         * + 16 bytes for algorithm ID
         * + 2 bytes for length of pown
         * + 2 bytes for length of challenge
         * + 2 bytes for length of additional parameters
         */
        if (r->recv_len < UDS_0X29_REQ_MIN_LEN + 16 + 2 + 2 + 2) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.verifyPownArgs.algoInd = &r->recv_buf[2];

        args.subFuncArgs.verifyPownArgs.pownLen =
            (uint16_t)((uint16_t)(r->recv_buf[18] << 8) | (uint16_t)r->recv_buf[19]);
        args.subFuncArgs.verifyPownArgs.pown = &r->recv_buf[20];

        if (args.subFuncArgs.verifyPownArgs.pownLen == 0) {
            UDS_LOGW(__FILE__, "Auth: VPOWNU/B with zero pown length\n");
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.verifyPownArgs.challengeLen =
            (uint16_t)((uint16_t)(r->recv_buf[20 + args.subFuncArgs.verifyPownArgs.pownLen] << 8) |
                       (uint16_t)r->recv_buf[21 + args.subFuncArgs.verifyPownArgs.pownLen]);
        args.subFuncArgs.verifyPownArgs.challenge =
            &r->recv_buf[22 + args.subFuncArgs.verifyPownArgs.pownLen];

        args.subFuncArgs.verifyPownArgs.addParamLen =
            (uint16_t)((uint16_t)(r->recv_buf[22 + args.subFuncArgs.verifyPownArgs.pownLen +
                                              args.subFuncArgs.verifyPownArgs.challengeLen]
                                  << 8) |
                       (uint16_t)r->recv_buf[23 + args.subFuncArgs.verifyPownArgs.pownLen +
                                             args.subFuncArgs.verifyPownArgs.challengeLen]);
        args.subFuncArgs.verifyPownArgs.addParam =
            &r->recv_buf[24 + args.subFuncArgs.verifyPownArgs.pownLen +
                         args.subFuncArgs.verifyPownArgs.challengeLen];

        memcpy(&r->send_buf[3], args.subFuncArgs.verifyPownArgs.algoInd, 16);
        r->send_len += 16;
        break;
    default:
        return NegativeResponse(r, UDS_NRC_SubFunctionNotSupported);
    }

    ret = EmitEvent(srv, UDS_EVT_Auth, &args);

    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    if (r->send_len < UDS_0X29_RESP_BASE_LEN) {
        goto respond_to_0x29_malformed_response;
    }

    switch (type) {
    case UDS_LEV_AT_DA:
    case UDS_LEV_AT_TC:
    case UDS_LEV_AT_AC:
        if (r->send_len < UDS_0X29_RESP_BASE_LEN) {
            goto respond_to_0x29_malformed_response;
        }
        break;
    case UDS_LEV_AT_VCU: {
        /**
         * + 2 bytes for length of challenge
         * + 2 bytes for length of ephemeral public key
         */
        if (r->send_len < UDS_0X29_RESP_BASE_LEN + 4) {
            goto respond_to_0x29_malformed_response;
        }

        uint16_t challengeLength =
            (uint16_t)((uint16_t)(r->send_buf[3] << 8) | (uint16_t)r->send_buf[4]);
        uint16_t pubKeyLength = (uint16_t)((uint16_t)(r->send_buf[5 + challengeLength] << 8) |
                                           (uint16_t)r->send_buf[6 + challengeLength]);

        if (challengeLength == 0) {
            UDS_LOGW(__FILE__, "Auth: VCU response with zero challenge length\n");
            goto respond_to_0x29_malformed_response;
        }

        if (challengeLength + pubKeyLength + UDS_0X29_RESP_BASE_LEN + 4 != r->send_len) {
            UDS_LOGW(__FILE__, "Auth: VCU response with malformed length\n");
            goto respond_to_0x29_malformed_response;
        }
        break;
    }
    case UDS_LEV_AT_VCB: {
        /**
         * + 2 bytes for length of challenge
         * + 2 bytes for length of certificate
         * + 2 bytes for length of pown
         * + 2 bytes for length of ephemeral public key
         */
        if (r->send_len < UDS_0X29_RESP_BASE_LEN + 8) {
            goto respond_to_0x29_malformed_response;
        }

        uint16_t challengeLength =
            (uint16_t)((uint16_t)(r->send_buf[3] << 8) | (uint16_t)r->send_buf[4]);

        if (challengeLength == 0) {
            UDS_LOGW(__FILE__, "Auth: VCB response with zero challenge length\n");
            goto respond_to_0x29_malformed_response;
        }

        uint16_t certLength = (uint16_t)((uint16_t)(r->send_buf[5 + challengeLength] << 8) |
                                         (uint16_t)r->send_buf[6 + challengeLength]);

        if (certLength == 0) {
            UDS_LOGW(__FILE__, "Auth: VCB response with zero certificate length\n");
            goto respond_to_0x29_malformed_response;
        }

        uint16_t pownLength =
            (uint16_t)((uint16_t)(r->send_buf[7 + challengeLength + certLength] << 8) |
                       (uint16_t)r->send_buf[8 + challengeLength + certLength]);
        if (pownLength == 0) {
            UDS_LOGW(__FILE__, "Auth: VCB response with zero pown length\n");
            goto respond_to_0x29_malformed_response;
        }

        uint16_t pubKeyLength =
            (uint16_t)((uint16_t)(r->send_buf[9 + challengeLength + certLength + pownLength] << 8) |
                       (uint16_t)r->send_buf[10 + challengeLength + certLength + pownLength]);
        if (pubKeyLength == 0) {
            UDS_LOGW(__FILE__, "Auth: VCB response with zero pubkey length\n");
            goto respond_to_0x29_malformed_response;
        }

        if (challengeLength + certLength + pownLength + pubKeyLength + UDS_0X29_RESP_BASE_LEN + 8 !=
            r->send_len) {
            UDS_LOGW(__FILE__, "Auth: VCB response with malformed length\n");
            goto respond_to_0x29_malformed_response;
        }

        break;
    }

    case UDS_LEV_AT_POWN: {
        /**
         * + 2 bytes for length of session key info
         */
        if (r->send_len < UDS_0X29_RESP_BASE_LEN + 2) {
            goto respond_to_0x29_malformed_response;
        }

        uint16_t sessionKeyInfoLength =
            (uint16_t)((uint16_t)(r->send_buf[3] << 8) | (uint16_t)r->send_buf[4]);

        if (sessionKeyInfoLength + UDS_0X29_RESP_BASE_LEN + 2 != r->send_len) {
            UDS_LOGW(__FILE__, "Auth: POWN response with malformed length\n");
            goto respond_to_0x29_malformed_response;
        }
        break;
    }
    case UDS_LEV_AT_RCFA: {
        /**
         * + 16 bytes for algorithm ID
         * + 2 bytes for length of challenge
         * + 0 bytes for challenge
         * + 2 bytes for length of additional parameters
         * + 0 bytes for additional parameters
         */
        if (r->send_len < UDS_0X29_RESP_BASE_LEN + 16 + 2 + 2) {
            goto respond_to_0x29_malformed_response;
        }

        uint16_t challengeLength =
            (uint16_t)((uint16_t)(r->send_buf[3 + 16] << 8) | (uint16_t)r->send_buf[4 + 16]);
        if (challengeLength == 0) {
            UDS_LOGW(__FILE__, "Auth: RCFA response with zero challenge length\n");
            goto respond_to_0x29_malformed_response;
        }

        uint16_t additionalParamLength =
            (uint16_t)((uint16_t)(r->send_buf[5 + 16 + challengeLength] << 8) |
                       (uint16_t)r->send_buf[6 + 16 + challengeLength]);

        if (r->send_len !=
            UDS_0X29_RESP_BASE_LEN + 16 + 2 + challengeLength + 2 + additionalParamLength) {
            UDS_LOGW(__FILE__, "Auth: RCFA response with malformed length\n");
            goto respond_to_0x29_malformed_response;
        }

        break;
    }

    case UDS_LEV_AT_VPOWNU: {
        /**
         * + 16 bytes for algorithm ID
         * + 2 bytes for length of session key
         */
        if (r->send_len < UDS_0X29_RESP_BASE_LEN + 16 + 2) {
            goto respond_to_0x29_malformed_response;
        }

        uint16_t sessionKeyLength =
            (uint16_t)((uint16_t)(r->send_buf[3 + 16] << 8) | (uint16_t)r->send_buf[4 + 16]);

        if (UDS_0X29_RESP_BASE_LEN + 16 + 2 + sessionKeyLength != r->send_len) {
            UDS_LOGW(__FILE__, "Auth: VPOWNU response with malformed length\n");
            goto respond_to_0x29_malformed_response;
        }

        break;
    }
    case UDS_LEV_AT_VPOWNB: {
        /**
         * + 16 bytes for algorithm ID
         * + 2 bytes for length of pown
         * + 2 bytes for length of session key
         */
        if (r->send_len < UDS_0X29_RESP_BASE_LEN + 16 + 2 + 2) {
            goto respond_to_0x29_malformed_response;
        }

        uint16_t pownLength =
            (uint16_t)((uint16_t)(r->send_buf[3 + 16] << 8) | (uint16_t)r->send_buf[4 + 16]);
        if (pownLength == 0) {
            UDS_LOGW(__FILE__, "Auth: VPOWNB response with zero pown length\n");
            goto respond_to_0x29_malformed_response;
        }

        uint16_t sessionKeyLength = (uint16_t)((uint16_t)(r->send_buf[5 + 16 + pownLength] << 8) |
                                               (uint16_t)r->send_buf[6 + 16 + pownLength]);
        if (UDS_0X29_RESP_BASE_LEN + 16 + 2 + pownLength + 2 + sessionKeyLength != r->send_len) {
            UDS_LOGW(__FILE__, "Auth: VPOWNB response with malformed length\n");
            goto respond_to_0x29_malformed_response;
        }

        break;
    }
    default:
        UDS_LOGW(__FILE__, "Auth: subFunc 0x%02X is not supported.\n", type);
        return NegativeResponse(r, UDS_NRC_SubFunctionNotSupported);
    }

    return UDS_PositiveResponse;

respond_to_0x29_malformed_response:
    UDS_LOGE(__FILE__, "Auth: subFunc 0x%02X is malformed. Length: %d\n", type, r->send_len);
    return NegativeResponse(r, UDS_NRC_GeneralReject);
}

static UDSErr_t Handle_0x2C_DynamicDefineDataIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    uint8_t type = r->recv_buf[1];

    if (r->recv_len < UDS_0X2C_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER);
    r->send_buf[1] = type;
    /* Set dynamicDataId. If response does not require it, the length will be adjusted later */
    r->send_buf[2] = r->recv_buf[2];
    r->send_buf[3] = r->recv_buf[3];
    r->send_len = UDS_0X2C_RESP_BASE_LEN + 2;

    UDSDDDIArgs_t args = {
        .type = type,
        .allDataIds = false,
        .dynamicDataId =
            (uint16_t)((uint16_t)r->recv_buf[2] << 8 | (uint16_t)r->recv_buf[3]) & 0xFFFF,
    };

    /* Since the paramter for subFunc 0x01 and 0x02 are dynamic and should not be handled by
     * separate events, we need to emit the event for every subfunction separatedly
     */
    switch (type) {
    case 0x01: /* defineByIdentifier */
    {
        if (r->recv_len < UDS_0X2C_REQ_MIN_LEN + 2 + 4 ||
            (r->recv_len - (UDS_0X2C_REQ_MIN_LEN + 2)) % 4 != 0) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        size_t numDIDs = (r->recv_len - 4) / 4;

        for (size_t i = 0; i < numDIDs; i++) {
            args.subFuncArgs.defineById.sourceDataId =
                (uint16_t)((uint16_t)r->recv_buf[4 + i * 4] << 8 |
                           (uint16_t)r->recv_buf[5 + i * 4]) &
                0xFFFF;
            args.subFuncArgs.defineById.position = r->recv_buf[6 + i * 4];
            args.subFuncArgs.defineById.size = r->recv_buf[7 + i * 4];

            ret = EmitEvent(srv, UDS_EVT_DynamicDefineDataId, &args);

            if (UDS_PositiveResponse != ret) {
                return NegativeResponse(r, ret);
            }
        }

        return UDS_PositiveResponse;
    }
    case 0x02: /* defineByMemoryAddress */
    {
        /* 2 bytes dynamic data id
         * 1 byte address and length format identifier
         * min 1 byte address
         * min 1 byte length
         */
        if (r->recv_len < UDS_0X2C_REQ_MIN_LEN + 5) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        size_t bytesPerAddrAndSize = ((r->recv_buf[4] & 0xF0) >> 4) + (r->recv_buf[4] & 0x0F);

        if (bytesPerAddrAndSize == 0) {
            UDS_LOGW(__FILE__,
                     "DDDI: define By Memory Address request with invalid "
                     "AddressAndLengthFormatIdentifier: 0x%02X\n",
                     r->recv_buf[4]);
            return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
        }
        if ((r->recv_len - 5) % bytesPerAddrAndSize != 0) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        size_t numAddrs = (r->recv_len - 5) / bytesPerAddrAndSize;

        for (size_t i = 0; i < numAddrs; i++) {
            ret = decodeAddressAndLengthWithOffset(r, &r->recv_buf[4],
                                                   &args.subFuncArgs.defineByMemAddress.memAddr,
                                                   &args.subFuncArgs.defineByMemAddress.memSize, i);

            if (UDS_PositiveResponse != ret) {
                return NegativeResponse(r, ret);
            }

            ret = EmitEvent(srv, UDS_EVT_DynamicDefineDataId, &args);

            if (UDS_PositiveResponse != ret) {
                return NegativeResponse(r, ret);
            }
        }

        return UDS_PositiveResponse;
    }

    case 0x03: /* clearDynamicallyDefined */
    {
        if (r->recv_len == UDS_0X2C_REQ_MIN_LEN) {
            args.allDataIds = true;
            r->send_len = UDS_0X2C_RESP_BASE_LEN;
        }

        ret = EmitEvent(srv, UDS_EVT_DynamicDefineDataId, &args);
        if (UDS_PositiveResponse != ret) {
            return NegativeResponse(r, ret);
        }

        return UDS_PositiveResponse;
    }
    default:
        UDS_LOGW(__FILE__, "Unsupported DDDI subFunc 0x%02X\n", type);
        return NegativeResponse(r, UDS_NRC_SubFunctionNotSupported);
    }
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

static UDSErr_t Handle_0x2F_IOControlByIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X2F_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_IO_CONTROL_BY_IDENTIFIER);
    r->send_buf[1] = r->recv_buf[1];
    r->send_buf[2] = r->recv_buf[2];
    r->send_buf[3] = r->recv_buf[3];
    r->send_len = UDS_0X2F_RESP_BASE_LEN;

    UDSIOCtrlArgs_t args = {
        .dataId = (uint16_t)(r->recv_buf[1] << 8) | (uint16_t)r->recv_buf[2],
        .ioCtrlParam = r->recv_buf[3],
        .ctrlStateAndMask = &r->recv_buf[UDS_0X2F_REQ_MIN_LEN],
        .ctrlStateAndMaskLen = r->recv_len - UDS_0X2F_REQ_MIN_LEN,
        .copy = safe_copy,
    };

    UDSErr_t err = EmitEvent(srv, UDS_EVT_IOControl, &args);

    if (err != UDS_PositiveResponse) {
        return NegativeResponse(r, err);
    }

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
        return NegativeResponse(r, UDS_NRC_GeneralReject);
    }

    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    ResetTransfer(srv);
    srv->xferIsActive = true;
    srv->xferTotalBytes = memorySize;
    srv->xferBlockLength = args.maxNumberOfBlockLength;

    // ISO-14229-1:2013 Table 401:
    uint8_t lengthFormatIdentifier = (uint8_t)(sizeof(args.maxNumberOfBlockLength) << 4);

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
        uint8_t shiftBytes = (uint8_t)(sizeof(args.maxNumberOfBlockLength) - 1 - idx);
        uint8_t byte = (args.maxNumberOfBlockLength >> (shiftBytes * 8)) & 0xFF;
        r->send_buf[UDS_0X34_RESP_BASE_LEN + idx] = byte;
    }
    r->send_len = UDS_0X34_RESP_BASE_LEN + (size_t)sizeof(args.maxNumberOfBlockLength);
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
        return NegativeResponse(r, UDS_NRC_GeneralReject);
    }

    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    ResetTransfer(srv);
    srv->xferIsActive = true;
    srv->xferTotalBytes = memorySize;
    srv->xferBlockLength = args.maxNumberOfBlockLength;

    uint8_t lengthFormatIdentifier = (uint8_t)(sizeof(args.maxNumberOfBlockLength) << 4);

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_UPLOAD);
    r->send_buf[1] = lengthFormatIdentifier;
    for (uint8_t idx = 0; idx < (uint8_t)sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = (sizeof(args.maxNumberOfBlockLength) - 1 - idx) & 0xFF;
        uint8_t byte = (args.maxNumberOfBlockLength >> (shiftBytes * 8)) & 0xFF;
        r->send_buf[UDS_0X35_RESP_BASE_LEN + idx] = byte;
    }
    r->send_len = UDS_0X35_RESP_BASE_LEN + (size_t)sizeof(args.maxNumberOfBlockLength);
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
        // (ReadDir) this parameter [fileSizeParameterLength] shall not be included in the
        // request message. If the modeOfOperation parameter equals to 0x02 (DeleteFile), 0x04
        // (ReadFile) or 0x05 (ReadDir) this parameter [fileSizeUncompressed] shall not be
        // included in the request message. If the modeOfOperation parameter equals to 0x02
        // (DeleteFile), 0x04 (ReadFile) or 0x05 (ReadDir) this parameter [fileSizeCompressed]
        // shall not be included in the request message.
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
            uint8_t shift_by_bytes = (uint8_t)(file_size_parameter_length - i - 1);
            file_size_uncompressed |= (size_t)data_byte << (8 * shift_by_bytes);
            byte_idx++;
        }
        for (size_t i = 0; i < file_size_parameter_length; i++) {
            uint8_t data_byte = r->recv_buf[byte_idx];
            uint8_t shift_by_bytes = (uint8_t)(file_size_parameter_length - i - 1);
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
    r->send_buf[2] = (uint8_t)sizeof(args.maxNumberOfBlockLength);
    for (uint8_t idx = 0; idx < (uint8_t)sizeof(args.maxNumberOfBlockLength); idx++) {
        uint8_t shiftBytes = (uint8_t)(sizeof(args.maxNumberOfBlockLength) - 1 - idx);
        uint8_t byte = (uint8_t)(args.maxNumberOfBlockLength >> (shiftBytes * 8));
        r->send_buf[UDS_0X38_RESP_BASE_LEN + idx] = byte;
    }
    r->send_buf[UDS_0X38_RESP_BASE_LEN + (size_t)sizeof(args.maxNumberOfBlockLength)] =
        args.dataFormatIdentifier;

    r->send_len = UDS_0X38_RESP_BASE_LEN + (size_t)sizeof(args.maxNumberOfBlockLength) + 1;
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

    uint8_t type = r->recv_buf[1] & 0x7F;

    UDSControlDTCSettingArgs_t args = {
        .type = type,
        .data = r->recv_len > UDS_0X85_REQ_BASE_LEN ? &r->recv_buf[UDS_0X85_REQ_BASE_LEN] : NULL,
        .len = r->recv_len > UDS_0X85_REQ_BASE_LEN ? r->recv_len - UDS_0X85_REQ_BASE_LEN : 0,
    };

    int ret = EmitEvent(srv, UDS_EVT_ControlDTCSetting, &args);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_CONTROL_DTC_SETTING);
    r->send_buf[1] = type;
    r->send_len = UDS_0X85_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x87_LinkControl(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X85_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t type = r->recv_buf[1] & 0x7F;

    if (type == 0x03 && (r->recv_buf[1] & 0x80) == 0 &&
        r->info.A_TA_Type == UDS_A_TA_TYPE_FUNCTIONAL) {
        UDS_LOGW(__FILE__, "0x87 LinkControl: Transitioning mode without suppressing response!");
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_LINK_CONTROL);
    r->send_buf[1] = r->recv_buf[1]; /* do not use `type` because we want to preserve the
                                        suppress response bit */
    r->send_len = UDS_0X87_RESP_LEN;

    UDSLinkCtrlArgs_t args = {
        .type = type,
        .len = (r->recv_len - UDS_0X87_REQ_BASE_LEN),
        .data = &r->recv_buf[UDS_0X87_REQ_BASE_LEN],
    };

    int ret = EmitEvent(srv, UDS_EVT_LinkControl, &args);
    if (ret != UDS_PositiveResponse) {
        return NegativeResponse(r, ret);
    }

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
        return &Handle_0x10_DiagnosticSessionControl;
    case kSID_ECU_RESET:
        return &Handle_0x11_ECUReset;
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
        return &Handle_0x14_ClearDiagnosticInformation;
    case kSID_READ_DTC_INFORMATION:
        return Handle_0x19_ReadDTCInformation;
    case kSID_READ_DATA_BY_IDENTIFIER:
        return &Handle_0x22_ReadDataByIdentifier;
    case kSID_READ_MEMORY_BY_ADDRESS:
        return &Handle_0x23_ReadMemoryByAddress;
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
        return NULL;
    case kSID_SECURITY_ACCESS:
        return &Handle_0x27_SecurityAccess;
    case kSID_COMMUNICATION_CONTROL:
        return &Handle_0x28_CommunicationControl;
    case kSID_AUTHENTICATION:
        return &Handle_0x29_Authentication;
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
        return NULL;
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
        return &Handle_0x2C_DynamicDefineDataIdentifier;
    case kSID_WRITE_DATA_BY_IDENTIFIER:
        return &Handle_0x2E_WriteDataByIdentifier;
    case kSID_IO_CONTROL_BY_IDENTIFIER:
        return &Handle_0x2F_IOControlByIdentifier;
    case kSID_ROUTINE_CONTROL:
        return &Handle_0x31_RoutineControl;
    case kSID_REQUEST_DOWNLOAD:
        return &Handle_0x34_RequestDownload;
    case kSID_REQUEST_UPLOAD:
        return &Handle_0x35_RequestUpload;
    case kSID_TRANSFER_DATA:
        return &Handle_0x36_TransferData;
    case kSID_REQUEST_TRANSFER_EXIT:
        return &Handle_0x37_RequestTransferExit;
    case kSID_REQUEST_FILE_TRANSFER:
        return &Handle_0x38_RequestFileTransfer;
    case kSID_WRITE_MEMORY_BY_ADDRESS:
        return &Handle_0x3D_WriteMemoryByAddress;
    case kSID_TESTER_PRESENT:
        return &Handle_0x3E_TesterPresent;
    case kSID_ACCESS_TIMING_PARAMETER:
        return NULL;
    case kSID_SECURED_DATA_TRANSMISSION:
        return NULL;
    case kSID_CONTROL_DTC_SETTING:
        return &Handle_0x85_ControlDTCSetting;
    case kSID_RESPONSE_ON_EVENT:
        return NULL;
    case kSID_LINK_CONTROL:
        return &Handle_0x87_LinkControl;
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
    case kSID_CONTROL_DTC_SETTING:
    case kSID_LINK_CONTROL: {
        UDS_ASSERT(service);
        response = service(srv, r);

        bool suppressPosRspMsgIndicationBit = r->recv_buf[1] & 0x80;

        /* test if positive response is required and if responseCode is positive 0x00 */
        if (suppressPosRspMsgIndicationBit && (response == UDS_PositiveResponse) &&

            // TODO: *not yet a NRC 0x78 response sent*
            true) {
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
        UDS_ASSERT(service);
        response = service(srv, r);
        break;
    }

    /* CASE Service_optional */
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
    case kSID_READ_DTC_INFORMATION:
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
    case kSID_IO_CONTROL_BY_IDENTIFIER:
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

        // TODO: *not yet a NRC 0x78 response sent*
        true) {
        /* Suppress negative response message */
        suppressResponse = true;
    }

    if (suppressResponse) {
        NoResponse(r);
    } else { /* send negative or positive response */
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
        EmitEvent(srv, UDS_EVT_AuthTimeout, NULL);
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
