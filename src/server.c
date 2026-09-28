#include "server.h"
#include "config.h"
#include "uds.h"
#include "uds_private.h"
#include "util.h"
#include "log.h"
#include "util_static.c"

static inline UDSErr_t NegativeResponse(UDSReq_t *r, const UDSErr_t nrc) {
    UDSErr_t ret = nrc;
    if ((nrc < UDS_PositiveResponse) || (nrc > UDS_NRC_UNUSED_ISOSAEReserved)) {
        UDS_LOGE(__FILE__, "Invalid negative response code: %d (0x%x)", nrc, nrc);
        ret = UDS_NRC_GeneralReject;
    }

    r->send_buf[0] = 0x7FU;
    r->send_buf[1] = r->recv_buf[0];
    r->send_buf[2] = (uint8_t)ret;
    r->send_len = UDS_NEG_RESP_LEN;
    return ret;
}

static inline void NoResponse(UDSReq_t *r) { r->send_len = 0; }

static UDSErr_t EmitEvent(UDSServer_t *srv, UDSEvent_t evt, void *data) {
    UDSErr_t err = UDS_OK;
    if (srv->fn != NULL) {
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

    uint8_t sessType = r->recv_buf[1] & 0x7FU;

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

    r->send_buf[0] = AsResponseSID(UDS_SID_DIAGNOSTIC_SESSION_CONTROL);
    r->send_buf[1] = sessType;

    // UDS-1-2013: Table 29
    // resolution: 1ms
    PackBE(&r->send_buf[2], args.p2_ms, 2);

    // resolution: 10ms
    PackBE(&r->send_buf[4], args.p2_star_ms / 10U, 2);

    r->send_len = UDS_0X10_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x11_ECUReset(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X11_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t resetType = r->recv_buf[1] & 0x3FU;

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

    r->send_buf[0] = AsResponseSID(UDS_SID_ECU_RESET);
    r->send_buf[1] = resetType;

    if (UDS_LEV_RT_ERPSD == resetType) {
        uint32_t powerDownTime = args.powerDownTimeMillis / 1000U;
        if (powerDownTime > 255U) {
            powerDownTime = 255U;
        }
        r->send_buf[2] = powerDownTime & 0xFFU;
        r->send_len = UDS_0X11_RESP_BASE_LEN + 1U;
    } else {
        r->send_len = UDS_0X11_RESP_BASE_LEN;
    }
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x14_ClearDiagnosticInformation(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X14_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION);
    r->send_len = UDS_0X14_RESP_BASE_LEN;

    UDSCDIArgs_t args = {
        .groupOfDTC = UnpackBEu32(&r->recv_buf[1], 3),
        .hasMemorySelection = (r->recv_len >= 5U),
        .memorySelection = (r->recv_len >= 5U) ? r->recv_buf[4] : 0U,
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
    UDSReq_t *r = &srv->r;
    if (count <= sizeof(r->send_buf) - r->send_len) {
        (void)memmove(&r->send_buf[r->send_len], src, count);
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
    r->send_buf[0] = AsResponseSID(UDS_SID_READ_DTC_INFORMATION);
    r->send_buf[1] = type;
    r->send_len = UDS_0X19_RESP_BASE_LEN;

    UDSRDTCIArgs_t args = {
        .type = type,
        .copy = safe_copy,
    };

    /* Before checks and emitting Request */
    switch (type) {
    case 0x01: /* reportNumberOfDTCByStatusMask */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 1U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.numOfDTCByStatusMaskArgs.mask = r->recv_buf[2];
        break;
    case 0x02: /* reportDTCByStatusMask */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 1U)) {
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
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 4U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcSnapshotRecordbyDTCNumArgs.dtc = UnpackBEu32(&r->recv_buf[2], 3);
        args.subFuncArgs.dtcSnapshotRecordbyDTCNumArgs.snapshotNum = r->recv_buf[5];
        break;
    case 0x05: /* reportDTCStoredDataByRecordNumber */
    case 0x16: /* reportDTCExtDataRecordByNumber */
    case 0x1A: /* reportDTCExtendedDataRecordIdentification */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 1U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcStoredDataByRecordNumArgs.recordNum = r->recv_buf[2];
        break;
    case 0x06: /* reportDTCExtDataRecordByDTCNumber */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 4U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.dtcExtDtaRecordByDTCNumArgs.dtc = UnpackBEu32(&r->recv_buf[2], 3);
        args.subFuncArgs.dtcExtDtaRecordByDTCNumArgs.extDataRecNum = r->recv_buf[5];
        break;
    case 0x07: /* reportNumberOfDTCBySeverityMaskRecord */
    case 0x08: /* reportDTCBySeverityMaskRecord */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 2U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.numOfDTCBySeverityMaskArgs.severityMask = r->recv_buf[2];
        args.subFuncArgs.numOfDTCBySeverityMaskArgs.statusMask = r->recv_buf[3];
        break;
    case 0x09: /* reportSeverityInformationOfDTC */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 1U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.severityInfoOfDTCArgs.dtc = UnpackBEu32(&r->recv_buf[2], 3);
        break;
    case 0x17: /* reportUserDefMemoryDTCByStatusMask */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 2U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.userDefMemoryDTCByStatusMaskArgs.mask = r->recv_buf[2];
        args.subFuncArgs.userDefMemoryDTCByStatusMaskArgs.memory = r->recv_buf[3];
        break;
    case 0x18: /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 5U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.dtc =
            UnpackBEu32(&r->recv_buf[2], 3);
        args.subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.snapshotNum = r->recv_buf[5];
        args.subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.memory = r->recv_buf[6];
        break;
    case 0x19: /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 5U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.dtc =
            UnpackBEu32(&r->recv_buf[2], 3);
        args.subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.extDataRecNum = r->recv_buf[5];
        args.subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.memory = r->recv_buf[6];
        break;
    case 0x42: /* reportWWHOBDDTCByMaskRecord */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 3U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.wwhobdDTCByMaskArgs.functionalGroup = r->recv_buf[2];
        args.subFuncArgs.wwhobdDTCByMaskArgs.statusMask = r->recv_buf[3];
        args.subFuncArgs.wwhobdDTCByMaskArgs.severityMask = r->recv_buf[4];
        break;
    case 0x55: /* reportWWHOBDDTCWithPermanentStatus */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 1U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        args.subFuncArgs.wwhobdDTCWithPermStatusArgs.functionalGroup = r->recv_buf[2];
        break;
    case 0x56: /* reportDTCInformationByDTCReadinessGroupIdentifier */
        if (r->recv_len < (UDS_0X19_REQ_MIN_LEN + 2U)) {
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
        if (r->send_len != (UDS_0X19_RESP_BASE_LEN + 4U)) {
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
        if ((r->send_len < (UDS_0X19_RESP_BASE_LEN + 1U)) ||
            ((r->send_len > (UDS_0X19_RESP_BASE_LEN + 1U)) &&
             (((r->send_len - UDS_0X19_RESP_BASE_LEN - 1U) % 4U) != 0U))) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x03: /* reportDTCSnapshotIdentification */
    case 0x14: /* reportDTCFaultDetectionCounter */
        if (((r->send_len - UDS_0X19_RESP_BASE_LEN) % 4U) != 0U) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x04: /* reportDTCSnapshotRecordByDTCNumber */
    case 0x06: /* reportDTCExtDataRecordByDTCNumber */
        if (r->send_len < (UDS_0X19_RESP_BASE_LEN + 4U)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x05: /* reportDTCStoredDataByRecordNumber */
    case 0x16: /* reportDTCExtDataRecordByNumber */
        if (r->send_len < (UDS_0X19_RESP_BASE_LEN + 1U)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x08: /* reportDTCBySeverityMaskRecord */
    case 0x09: /* reportSeverityInformationOfDTC */
        if ((r->send_len < (UDS_0X19_RESP_BASE_LEN + 1U)) ||
            ((r->send_len > (UDS_0X19_RESP_BASE_LEN + 1U)) &&
             (((r->send_len - UDS_0X19_RESP_BASE_LEN - 1U) % 6U) != 0U))) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x17: /* reportUserDefMemoryDTCByStatusMask */
        if (r->send_len < (UDS_0X19_RESP_BASE_LEN + 2U) ||
            ((r->send_len > (UDS_0X19_RESP_BASE_LEN + 2U)) &&
             (((r->send_len - UDS_0X19_RESP_BASE_LEN + 2U) % 4U) != 0U))) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x18: /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
    case 0x19: /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        if (r->send_len < (UDS_0X19_RESP_BASE_LEN + 5U)) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x1A: /* reportDTCExtendedDataRecordIdentification */
        if ((r->send_len < (UDS_0X19_RESP_BASE_LEN + 1U)) ||
            ((r->send_len != (UDS_0X19_RESP_BASE_LEN + 6U)) &&
             (r->send_len > (UDS_0X19_RESP_BASE_LEN + 1U)) &&
             (r->send_len < (UDS_0X19_RESP_BASE_LEN + 4U))) ||
            ((r->send_len > (UDS_0X19_RESP_BASE_LEN + 6U)) &&
             (((r->send_len - UDS_0X19_RESP_BASE_LEN + 6U) % 4U) != 0U))) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x42: /* reportWWHOBDDTCByMaskRecord */
        if (r->send_len < (UDS_0X19_RESP_BASE_LEN + 4U) ||
            ((r->send_len > (UDS_0X19_RESP_BASE_LEN + 4U)) &&
             (((r->send_len - UDS_0X19_RESP_BASE_LEN - 4U) % 5U) != 0U))) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x55: /* reportWWHOBDDTCWithPermanentStatus */
        if ((r->send_len < (UDS_0X19_RESP_BASE_LEN + 3U)) ||
            ((r->send_len > (UDS_0X19_RESP_BASE_LEN + 3U)) &&
             (((r->send_len - UDS_0X19_RESP_BASE_LEN - 3U) % 4U) != 0U))) {
            goto respond_to_0x19_malformed_response;
        }
        break;
    case 0x56: /* reportDTCInformationByDTCReadinessGroupIdentifier */
        if ((r->send_len < (UDS_0X19_RESP_BASE_LEN + 4U)) ||
            ((r->send_len > (UDS_0X19_RESP_BASE_LEN + 4U)) &&
             (((r->send_len - UDS_0X19_RESP_BASE_LEN + 4U) % 4U) != 0U))) {
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
    if (0U != (r->recv_len - 1U) % sizeof(uint16_t)) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    const uint8_t numDIDs = (uint8_t)(r->recv_len / sizeof(uint16_t));

    if (0U == numDIDs) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_READ_DATA_BY_IDENTIFIER);
    r->send_len = 1;

    for (uint16_t did = 0; did < numDIDs; did++) {
        uint16_t idx = (uint16_t)(1U + (did * 2U));
        uint16_t dataId = UnpackBEu16(&r->recv_buf[idx]);

        if ((r->send_len + 3U) > sizeof(r->send_buf)) {
            return NegativeResponse(r, UDS_NRC_ResponseTooLong);
        }
        PackBE(&r->send_buf[r->send_len], dataId, 2);
        r->send_len += 2U;

        UDSRDBIArgs_t args = {
            .dataId = dataId,
            .copy = safe_copy,
        };

        const size_t send_len_before = r->send_len;
        UDSErr_t ret = EmitEvent(srv, UDS_EVT_ReadDataByIdent, &args);

        // r->send_len is modified when the user calls safe_copy.
        /* cppcheck-suppress [knownConditionTrueFalse] */
        if ((UDS_PositiveResponse == ret) && (send_len_before == r->send_len)) {
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
 * @brief decodes addressAndLength at a nonzero offset within the receive buffer.
 * @see ISO 14229-1:2020(E) Annex H
 *
 * @param r request buffer
 * @param buf pointer to addressAndDataLengthFormatIdentifier in recv_buf
 * @param memoryAddress the decoded memory address
 * @param memorySize the decoded memory size
 * @param offset how many elements (addres and size pairs) away from the format identifier
 * @return UDSErr_t
 */
static UDSErr_t decodeAddressAndLengthAt(UDSReq_t *r, uint8_t *const buf, uintptr_t *memoryAddress,
                                         size_t *memorySize, size_t offset) {
    UDS_ASSERT(r);
    UDS_ASSERT(memoryAddress);
    UDS_ASSERT(memorySize);
    *memoryAddress = 0;
    *memorySize = 0;

    if (r->recv_len < 3U) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    const uint8_t memorySizeLength = (buf[0] & 0xF0U) >> 4U;
    const uint8_t memoryAddressLength = buf[0] & 0x0FU;

    if ((memorySizeLength == 0U) || (memorySizeLength > sizeof(size_t))) {
        return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
    }

    if ((memoryAddressLength == 0U) || (memoryAddressLength > sizeof(size_t))) {
        return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
    }

    const size_t offsetBytes = offset * ((size_t)memoryAddressLength + (size_t)memorySizeLength);

    if ((1U + offsetBytes + memorySizeLength + memoryAddressLength) > r->recv_len) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    *memoryAddress = UnpackBEuintptr(&buf[1U + offsetBytes], memoryAddressLength);
    *memorySize = UnpackBEsize(&buf[1U + offsetBytes + memoryAddressLength], memorySizeLength);

    return UDS_PositiveResponse;
}

/**
 * @brief decode the addressAndLengthFormatIdentifier
 *
 * @param srv
 * @param buf pointer to addressAndDataLengthFormatIdentifier in recv_buf
 * @param memoryAddress the decoded memory address
 * @param memorySize the decoded memory size
 * @return uint8_t
 */
static UDSErr_t decodeAddressAndLength(UDSReq_t *r, uint8_t *const buf, uintptr_t *memoryAddress,
                                       size_t *memorySize) {
    return decodeAddressAndLengthAt(r, buf, memoryAddress, memorySize, 0);
}

static UDSErr_t Handle_0x23_ReadMemoryByAddress(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    uintptr_t address = 0;
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

    r->send_buf[0] = AsResponseSID(UDS_SID_READ_MEMORY_BY_ADDRESS);
    r->send_len = UDS_0X23_RESP_BASE_LEN;
    ret = EmitEvent(srv, UDS_EVT_ReadMemByAddr, &args);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }
    if (r->send_len != (UDS_0X23_RESP_BASE_LEN + length)) {
        UDS_LOGE(__FILE__, "response positive but not all data sent: expected %zu, sent %zu",
                 length, r->send_len - UDS_0X23_RESP_BASE_LEN);
        return NegativeResponse(r, UDS_NRC_GeneralReject);
    }
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x27_SecurityAccess(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X27_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    if (!UDSTimeAfter(UDSMillis(), srv->sec_access_boot_delay_timer)) {
        return NegativeResponse(r, UDS_NRC_RequiredTimeDelayNotExpired);
    }

    if (!UDSTimeAfter(UDSMillis(), srv->sec_access_auth_fail_timer)) {
        return NegativeResponse(r, UDS_NRC_ExceedNumberOfAttempts);
    }

    uint8_t subFunction = r->recv_buf[1];
    if (UDSSecurityAccessLevelIsReserved(subFunction)) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_SECURITY_ACCESS);
    r->send_buf[1] = subFunction;
    r->send_len = UDS_0X27_RESP_BASE_LEN;

    // Even: sendKey
    if (0U == (subFunction % 2U)) {
        UDS_ASSERT(subFunction >= 2U);
        uint8_t requestedLevel = subFunction - 1U;
        UDSSecAccessValidateKeyArgs_t args = {
            .level = requestedLevel,
            .key = &r->recv_buf[UDS_0X27_REQ_BASE_LEN],
            .len = (uint16_t)(r->recv_len - UDS_0X27_REQ_BASE_LEN),
        };

        UDSErr_t ret = EmitEvent(srv, UDS_EVT_SecAccessValidateKey, &args);

        if (UDS_PositiveResponse != ret) {
            srv->sec_access_auth_fail_timer =
                UDSMillis() + UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS;
            return NegativeResponse(r, ret);
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
    UDS_ASSERT(1U == (subFunction % 2U));

    // Already unlocked: send a zero seed
    if (subFunction == srv->securityLevel) {
        // Table 52 sends a response of length 2. Use a preprocessor define if this needs
        // customizing by the user.
        const uint8_t already_unlocked[] = {0x00, 0x00};
        return safe_copy(srv, already_unlocked, sizeof(already_unlocked));
    }

    // Not yet unlocked: request a seed
    UDS_ASSERT(subFunction != srv->securityLevel);

    UDSSecAccessRequestSeedArgs_t args = {
        .level = subFunction,
        .dataRecord = &r->recv_buf[UDS_0X27_REQ_BASE_LEN],
        .len = (uint16_t)(r->recv_len - UDS_0X27_REQ_BASE_LEN),
        .copySeed = safe_copy,
    };

    UDSErr_t ret = EmitEvent(srv, UDS_EVT_SecAccessRequestSeed, &args);

    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    if (r->send_len <= UDS_0X27_RESP_BASE_LEN) { // no data was copied
        UDS_LOGE(__FILE__, "0x27: no seed data was copied");
        return NegativeResponse(r, UDS_NRC_GeneralReject);
    }
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x28_CommunicationControl(UDSServer_t *srv, UDSReq_t *r) {
    uint8_t controlType = r->recv_buf[1] & 0x7FU;
    uint8_t communicationType = r->recv_buf[2];

    if (r->recv_len < UDS_0X28_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    UDSCommCtrlArgs_t args = {
        .ctrlType = controlType,
        .commType = communicationType,
        .nodeId = 0,
    };

    if ((args.ctrlType == 0x04U) || (args.ctrlType == 0x05U)) {
        if (r->recv_len < (UDS_0X28_REQ_BASE_LEN + 2U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }
        args.nodeId = UnpackBEu16(&r->recv_buf[3]);
    }

    UDSErr_t err = EmitEvent(srv, UDS_EVT_CommCtrl, &args);
    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_COMMUNICATION_CONTROL);
    r->send_buf[1] = controlType;
    r->send_len = UDS_0X28_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x2C_DynamicDefineDataIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    uint8_t type = r->recv_buf[1];

    if (r->recv_len < UDS_0X2C_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER);
    r->send_buf[1] = type;
    /* Set dynamicDataId. If response does not require it, the length will be adjusted later */
    r->send_buf[2] = r->recv_buf[2];
    r->send_buf[3] = r->recv_buf[3];
    r->send_len = UDS_0X2C_RESP_BASE_LEN + 2U;

    UDSDDDIArgs_t args = {
        .type = type,
        .allDataIds = false,
        .dynamicDataId = UnpackBEu16(&r->recv_buf[2]),
    };

    /* Since the paramter for subFunc 0x01 and 0x02 are dynamic and should not be handled by
     * separate events, we need to emit the event for every subfunction separatedly
     */
    switch (type) {
    case 0x01: /* defineByIdentifier */
    {
        if ((r->recv_len < (UDS_0X2C_REQ_MIN_LEN + 2U + 4U)) ||
            (((r->recv_len - (UDS_0X2C_REQ_MIN_LEN + 2U)) % 4U) != 0U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        size_t numDIDs = (r->recv_len - 4U) / 4U;

        for (size_t i = 0; i < numDIDs; i++) {
            args.subFuncArgs.defineById.sourceDataId = UnpackBEu16(&r->recv_buf[4U + (i * 4U)]);
            args.subFuncArgs.defineById.position = r->recv_buf[6U + (i * 4U)];
            args.subFuncArgs.defineById.size = r->recv_buf[7U + (i * 4U)];

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
        if (r->recv_len < (UDS_0X2C_REQ_MIN_LEN + 5U)) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        size_t bytesPerAddrAndSize = ((r->recv_buf[4] & 0xF0U) >> 4U) + (r->recv_buf[4] & 0x0FU);

        if (bytesPerAddrAndSize == 0U) {
            UDS_LOGW(__FILE__,
                     "DDDI: define By Memory Address request with invalid "
                     "AddressAndLengthFormatIdentifier: 0x%02X\n",
                     r->recv_buf[4]);
            return NegativeResponse(r, UDS_NRC_RequestOutOfRange);
        }

        if (((r->recv_len - 5U) % bytesPerAddrAndSize) != 0U) {
            return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
        }

        size_t numAddrs = (r->recv_len - 5U) / bytesPerAddrAndSize;

        for (size_t i = 0; i < numAddrs; i++) {
            ret = decodeAddressAndLengthAt(r, &r->recv_buf[4],
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

    dataId = UnpackBEu16(&r->recv_buf[1]);
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

    r->send_buf[0] = AsResponseSID(UDS_SID_WRITE_DATA_BY_IDENTIFIER);
    PackBE(&r->send_buf[1], dataId, 2);
    r->send_len = UDS_0X2E_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x2F_IOControlByIdentifier(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X2F_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_IO_CONTROL_BY_IDENTIFIER);
    r->send_buf[1] = r->recv_buf[1];
    r->send_buf[2] = r->recv_buf[2];
    r->send_buf[3] = r->recv_buf[3];
    r->send_len = UDS_0X2F_RESP_BASE_LEN;

    UDSIOCtrlArgs_t args = {
        .dataId = UnpackBEu16(&r->recv_buf[1]),
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

    uint8_t routineControlType = r->recv_buf[1] & 0x7FU;
    uint16_t routineIdentifier = UnpackBEu16(&r->recv_buf[2]);

    UDSRoutineCtrlArgs_t args = {
        .ctrlType = routineControlType,
        .id = routineIdentifier,
        .optionRecord = &r->recv_buf[UDS_0X31_REQ_MIN_LEN],
        .len = (uint16_t)(r->recv_len - UDS_0X31_REQ_MIN_LEN),
        .copyStatusRecord = safe_copy,
    };

    r->send_buf[0] = AsResponseSID(UDS_SID_ROUTINE_CONTROL);
    r->send_buf[1] = routineControlType;
    PackBE(&r->send_buf[2], routineIdentifier, 2);
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

static void BeginTransfer(UDSServer_t *srv, size_t xferTotalBytes, size_t xferBlockLength) {
    UDS_ASSERT(srv);
    srv->xferBlockSequenceCounter = 1;
    srv->xferByteCounter = 0;
    srv->xferTotalBytes = xferTotalBytes;
    srv->xferBlockLength = xferBlockLength;
    srv->xferIsActive = true;
}

static UDSErr_t Handle_0x34_RequestDownload(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;
    uintptr_t memoryAddress = 0;
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

    if (args.maxNumberOfBlockLength < 3U) {
        UDS_LOGE(__FILE__, "maxNumberOfBlockLength too short");
        return NegativeResponse(r, UDS_NRC_GeneralReject);
    }

    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    BeginTransfer(srv, memorySize, args.maxNumberOfBlockLength);

    // ISO-14229-1:2013 Table 401:
    uint8_t lengthFormatIdentifier = (uint8_t)(sizeof(args.maxNumberOfBlockLength) << 4U);

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

    r->send_buf[0] = AsResponseSID(UDS_SID_REQUEST_DOWNLOAD);
    r->send_buf[1] = lengthFormatIdentifier;
    enum { N = 2 };
    static_assert(sizeof(args.maxNumberOfBlockLength) == N, "See ISO14229-1:2020 Table 442");
    PackBE(&r->send_buf[2], args.maxNumberOfBlockLength, N);
    r->send_len = UDS_0X34_RESP_BASE_LEN + N;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x35_RequestUpload(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;
    uintptr_t memoryAddress = 0;
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

    if (args.maxNumberOfBlockLength < 3U) {
        UDS_LOGE(__FILE__, "maxNumberOfBlockLength too short");
        return NegativeResponse(r, UDS_NRC_GeneralReject);
    }

    if (UDS_PositiveResponse != err) {
        return NegativeResponse(r, err);
    }

    BeginTransfer(srv, memorySize, args.maxNumberOfBlockLength);

    uint8_t lengthFormatIdentifier = (uint8_t)(sizeof(args.maxNumberOfBlockLength) << 4U);

    r->send_buf[0] = AsResponseSID(UDS_SID_REQUEST_UPLOAD);
    r->send_buf[1] = lengthFormatIdentifier;
    PackBE(&r->send_buf[UDS_0X35_RESP_BASE_LEN], args.maxNumberOfBlockLength,
           sizeof(args.maxNumberOfBlockLength));
    r->send_len = UDS_0X35_RESP_BASE_LEN + (size_t)sizeof(args.maxNumberOfBlockLength);
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x36_TransferData(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;
    uint8_t blockSequenceCounter = 0;

    if (!srv->xferIsActive) {
        return NegativeResponse(r, UDS_NRC_UploadDownloadNotAccepted);
    }

    if (r->recv_len < UDS_0X36_REQ_BASE_LEN) {
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    uint16_t request_data_len = (uint16_t)(r->recv_len - UDS_0X36_REQ_BASE_LEN);
    blockSequenceCounter = r->recv_buf[1];

    if (!srv->RCRRP) {
        if (blockSequenceCounter != srv->xferBlockSequenceCounter) {
            err = UDS_NRC_RequestSequenceError;
            goto fail;
        } else {
            srv->xferBlockSequenceCounter++;
        }
    }

    if ((srv->xferByteCounter + request_data_len) > srv->xferTotalBytes) {
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

        r->send_buf[0] = AsResponseSID(UDS_SID_TRANSFER_DATA);
        r->send_buf[1] = blockSequenceCounter;
        r->send_len = UDS_0X36_RESP_BASE_LEN;

        err = EmitEvent(srv, UDS_EVT_TransferData, &args);

        if (err == UDS_PositiveResponse) {
            srv->xferByteCounter += request_data_len;
            return UDS_PositiveResponse;
        }
        if (err == UDS_NRC_RequestCorrectlyReceived_ResponsePending) {
            return NegativeResponse(r, UDS_NRC_RequestCorrectlyReceived_ResponsePending);
        }
        goto fail;
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

    r->send_buf[0] = AsResponseSID(UDS_SID_REQUEST_TRANSFER_EXIT);
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
    }
    if (err == UDS_NRC_RequestCorrectlyReceived_ResponsePending) {
        return NegativeResponse(r, UDS_NRC_RequestCorrectlyReceived_ResponsePending);
    }
    ResetTransfer(srv);
    return NegativeResponse(r, err);
}

static UDSErr_t Handle_0x38_RequestFileTransfer(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t err = UDS_PositiveResponse;

    if (srv->xferIsActive) {
        err = UDS_NRC_ConditionsNotCorrect;
        goto done;
    }
    if (r->recv_len < UDS_0X38_REQ_BASE_LEN) {
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto done;
    }

    const uint8_t mode_of_operation = r->recv_buf[1];

    if ((mode_of_operation < UDS_MOOP_ADDFILE) || (mode_of_operation > UDS_MOOP_RSFILE)) {
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto done;
    }

    const uint16_t file_path_len = UnpackBEu16(&r->recv_buf[2]);
    uint8_t data_format_identifier = 0;
    uint8_t file_size_parameter_length = 0; // also called "k" in ISO14229:2020
    size_t file_size_uncompressed = 0;
    size_t file_size_compressed = 0;
    size_t byte_idx = 4U + (size_t)file_path_len;

    if (byte_idx > r->recv_len) {
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto done;
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
        // Paraphrasing ISO14229:2020 Table 481:
        // If the modeOfOperation parameter equals to 0x02 (DeleteFile), 0x04 (ReadFile) or 0x05
        // (ReadDir) these parameters [fileSizeParameterLength, fileSizeUncompressed,
        // fileSizeCompressed] shall not be included in the request message.
    } else {
        file_size_parameter_length = r->recv_buf[byte_idx];
        byte_idx++;

        static_assert(sizeof(file_size_uncompressed) == sizeof(file_size_compressed),
                      "Both should be k-byte numbers per Table 480");
        if (file_size_parameter_length > sizeof(file_size_compressed)) {
            err = UDS_NRC_RequestOutOfRange;
            goto done;
        }
        // the remaining two request fields (fileSizeUncompressed and fileSizeCompressed) are each
        // file_size_parameter_length (k) bytes long
        if ((byte_idx + (2U * (size_t)file_size_parameter_length)) > r->recv_len) {
            err = UDS_NRC_RequestOutOfRange;
            goto done;
        }
        for (uint8_t i = 0; i < file_size_parameter_length; i++) {
            uint8_t data_byte = r->recv_buf[byte_idx];
            uint8_t shift_by_bytes = (uint8_t)(file_size_parameter_length - i - 1U);
            file_size_uncompressed |= (size_t)data_byte << (8U * (size_t)shift_by_bytes);
            byte_idx++;
        }
        for (uint8_t i = 0; i < file_size_parameter_length; i++) {
            uint8_t data_byte = r->recv_buf[byte_idx];
            uint8_t shift_by_bytes = (uint8_t)(file_size_parameter_length - i - 1U);
            file_size_compressed |= (size_t)data_byte << (8U * (size_t)shift_by_bytes);
            byte_idx++;
        }
    }

    UDSRequestFileTransferArgs_t args = {
        .modeOfOperation = mode_of_operation,
        .filePathLen = file_path_len,
        .filePath = (file_path_len == 0U) ? NULL : &r->recv_buf[4],
        .dataFormatIdentifier = data_format_identifier,
        .fileSizeUnCompressed = file_size_uncompressed,
        .fileSizeCompressed = file_size_compressed,
        .maxNumberOfBlockLength = UDS_TP_MTU,
        .filePosition = 0,
    };

    err = EmitEvent(srv, UDS_EVT_RequestFileTransfer, &args);

    if (UDS_PositiveResponse != err) {
        goto done;
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_REQUEST_FILE_TRANSFER);
    r->send_buf[1] = mode_of_operation;

    if (mode_of_operation == UDS_MOOP_DELFILE) {
        r->send_len = 2;
        goto done;
    }

    if (args.maxNumberOfBlockLength > UDS_TP_MTU) {
        UDS_LOGW(__FILE__, "Clamping maxNumberOfBlockLength %hu to %hu",
                 args.maxNumberOfBlockLength, UDS_TP_MTU);
        args.maxNumberOfBlockLength = UDS_TP_MTU;
    }

    BeginTransfer(srv, args.fileSizeCompressed, args.maxNumberOfBlockLength);

    // lengthFormatIdentifier: A_Data byte 3
    r->send_buf[2] = (uint8_t)sizeof(args.maxNumberOfBlockLength);
    r->send_len = 3;

    // A_Data bytes 4 to 4+m-1: maxNumberOfBlockLength
    PackBE(&r->send_buf[r->send_len], args.maxNumberOfBlockLength,
           sizeof(args.maxNumberOfBlockLength));
    r->send_len += (size_t)sizeof(args.maxNumberOfBlockLength);

    // daataFormatIdentifier: 0 if ReadDir
    r->send_buf[r->send_len] =
        (UDS_MOOP_RDDIR == mode_of_operation) ? 0x00U : args.dataFormatIdentifier;
    r->send_len += 1U;

    if ((mode_of_operation == UDS_MOOP_ADDFILE) || (mode_of_operation == UDS_MOOP_DELFILE) ||
        (mode_of_operation == UDS_MOOP_REPLFILE) || (mode_of_operation == UDS_MOOP_RSFILE)) {
        // pass
    } else {
        // fileSizeOrDirInfoParameterLength
        PackBE(&r->send_buf[r->send_len], sizeof(args.fileSizeUnCompressed), 2);
        r->send_len += 2U;

        // fileSizeUncompressedOrDirInfoLength
        PackBE(&r->send_buf[r->send_len], args.fileSizeUnCompressed,
               sizeof(args.fileSizeUnCompressed));
        r->send_len += sizeof(args.fileSizeUnCompressed);

        if (mode_of_operation == UDS_MOOP_RDDIR) {
            // pass
        } else {
            // fileSizeCompressed
            PackBE(&r->send_buf[r->send_len], args.fileSizeCompressed,
                   sizeof(args.fileSizeCompressed));
            r->send_len += sizeof(args.fileSizeCompressed);
        }
    }

    if ((mode_of_operation == UDS_MOOP_ADDFILE) || (mode_of_operation == UDS_MOOP_DELFILE) ||
        (mode_of_operation == UDS_MOOP_REPLFILE) || (mode_of_operation == UDS_MOOP_RDFILE) ||
        (mode_of_operation == UDS_MOOP_RDDIR)) {
        // pass
    } else {
        // filePosition
        PackBE(&r->send_buf[r->send_len], args.filePosition, sizeof(args.filePosition));
        r->send_len += sizeof(args.filePosition);
    }

done:
    return err;
}

static UDSErr_t Handle_0x3D_WriteMemoryByAddress(UDSServer_t *srv, UDSReq_t *r) {
    UDSErr_t ret = UDS_PositiveResponse;
    uintptr_t address = 0;
    size_t length = 0;

    if (r->recv_len < UDS_0X3D_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    ret = decodeAddressAndLength(r, &r->recv_buf[1], &address, &length);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    uint8_t memorySizeLength = (r->recv_buf[1] & 0xF0U) >> 4U;
    uint8_t memoryAddressLength = r->recv_buf[1] & 0x0FU;

    uint8_t dataOffset = 2U + memorySizeLength + memoryAddressLength;

    if ((dataOffset + length) != r->recv_len) {
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

    r->send_buf[0] = AsResponseSID(UDS_SID_WRITE_MEMORY_BY_ADDRESS);
    // echo addressAndLengthFormatIdentifier, memoryAddress, and memorySize
    (void)memcpy(&r->send_buf[1], &r->recv_buf[1], 1U + memorySizeLength + memoryAddressLength);
    r->send_len = UDS_0X3D_RESP_BASE_LEN + (size_t)memorySizeLength + (size_t)memoryAddressLength;
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
        r->send_buf[0] = AsResponseSID(UDS_SID_TESTER_PRESENT);
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

    uint8_t type = r->recv_buf[1] & 0x7FU;

    UDSControlDTCSettingArgs_t args = {
        .type = type,
        .data = (r->recv_len > UDS_0X85_REQ_BASE_LEN) ? &r->recv_buf[UDS_0X85_REQ_BASE_LEN] : NULL,
        .len = (r->recv_len > UDS_0X85_REQ_BASE_LEN) ? (r->recv_len - UDS_0X85_REQ_BASE_LEN) : 0U,
    };

    UDSErr_t ret = EmitEvent(srv, UDS_EVT_ControlDTCSetting, &args);
    if (UDS_PositiveResponse != ret) {
        return NegativeResponse(r, ret);
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_CONTROL_DTC_SETTING);
    r->send_buf[1] = type;
    r->send_len = UDS_0X85_RESP_LEN;
    return UDS_PositiveResponse;
}

static UDSErr_t Handle_0x87_LinkControl(UDSServer_t *srv, UDSReq_t *r) {
    if (r->recv_len < UDS_0X85_REQ_BASE_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t type = r->recv_buf[1] & 0x7FU;

    if ((type == 0x03U) && ((r->recv_buf[1] & 0x80U) == 0U) &&
        (r->info.A_TA_Type == UDS_A_TA_TYPE_FUNCTIONAL)) {
        UDS_LOGW(__FILE__, "0x87 LinkControl: Transitioning mode without suppressing response!");
    }

    r->send_buf[0] = AsResponseSID(UDS_SID_LINK_CONTROL);
    r->send_buf[1] = r->recv_buf[1]; /* do not use `type` because we want to preserve the suppress
                                        response bit */
    r->send_len = UDS_0X87_RESP_LEN;

    UDSLinkCtrlArgs_t args = {
        .type = type,
        .len = (r->recv_len - UDS_0X87_REQ_BASE_LEN),
        .data = &r->recv_buf[UDS_0X87_REQ_BASE_LEN],
    };

    UDSErr_t err = EmitEvent(srv, UDS_EVT_LinkControl, &args);
    if (err != UDS_PositiveResponse) {
        return NegativeResponse(r, err);
    }

    return UDS_PositiveResponse;
}

/// signature of internal service handlers
typedef UDSErr_t (*UDSService)(UDSServer_t *srv, UDSReq_t *r);

/**
 * @brief Get the internal service handler matching the given SID.
 * @param sid
 * @return pointer to UDSService or NULL if no match
 */
static UDSService getServiceForSID(uint8_t sid) {
    switch (sid) {
    case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
        return &Handle_0x10_DiagnosticSessionControl;
    case UDS_SID_ECU_RESET:
        return &Handle_0x11_ECUReset;
    case UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION:
        return &Handle_0x14_ClearDiagnosticInformation;
    case UDS_SID_READ_DTC_INFORMATION:
        return Handle_0x19_ReadDTCInformation;
    case UDS_SID_READ_DATA_BY_IDENTIFIER:
        return &Handle_0x22_ReadDataByIdentifier;
    case UDS_SID_READ_MEMORY_BY_ADDRESS:
        return &Handle_0x23_ReadMemoryByAddress;
    case UDS_SID_READ_SCALING_DATA_BY_IDENTIFIER:
        return NULL;
    case UDS_SID_SECURITY_ACCESS:
        return &Handle_0x27_SecurityAccess;
    case UDS_SID_COMMUNICATION_CONTROL:
        return &Handle_0x28_CommunicationControl;
    case UDS_SID_READ_PERIODIC_DATA_BY_IDENTIFIER:
        return NULL;
    case UDS_SID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
        return &Handle_0x2C_DynamicDefineDataIdentifier;
    case UDS_SID_WRITE_DATA_BY_IDENTIFIER:
        return &Handle_0x2E_WriteDataByIdentifier;
    case UDS_SID_IO_CONTROL_BY_IDENTIFIER:
        return &Handle_0x2F_IOControlByIdentifier;
    case UDS_SID_ROUTINE_CONTROL:
        return &Handle_0x31_RoutineControl;
    case UDS_SID_REQUEST_DOWNLOAD:
        return &Handle_0x34_RequestDownload;
    case UDS_SID_REQUEST_UPLOAD:
        return &Handle_0x35_RequestUpload;
    case UDS_SID_TRANSFER_DATA:
        return &Handle_0x36_TransferData;
    case UDS_SID_REQUEST_TRANSFER_EXIT:
        return &Handle_0x37_RequestTransferExit;
    case UDS_SID_REQUEST_FILE_TRANSFER:
        return &Handle_0x38_RequestFileTransfer;
    case UDS_SID_WRITE_MEMORY_BY_ADDRESS:
        return &Handle_0x3D_WriteMemoryByAddress;
    case UDS_SID_TESTER_PRESENT:
        return &Handle_0x3E_TesterPresent;
    case UDS_SID_ACCESS_TIMING_PARAMETER:
    case UDS_SID_SECURED_DATA_TRANSMISSION:
        return NULL;
    case UDS_SID_CONTROL_DTC_SETTING:
        return &Handle_0x85_ControlDTCSetting;
    case UDS_SID_RESPONSE_ON_EVENT:
        return NULL;
    case UDS_SID_LINK_CONTROL:
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

    if (NULL == srv->fn) {
        return NegativeResponse(r, UDS_NRC_ServiceNotSupported);
    }
    UDS_ASSERT(srv->fn); // service handler functions will call srv->fn. it must be valid

    switch (sid) {
    /* CASE Service_with_sub-function */
    /* test if service with sub-function is supported */
    case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
    case UDS_SID_ECU_RESET:
    case UDS_SID_SECURITY_ACCESS:
    case UDS_SID_COMMUNICATION_CONTROL:
    case UDS_SID_ROUTINE_CONTROL:
    case UDS_SID_TESTER_PRESENT:
    case UDS_SID_CONTROL_DTC_SETTING:
    case UDS_SID_LINK_CONTROL: {
        UDS_ASSERT(service);
        response = service(srv, r);

        bool suppressPosRspMsgIndicationBit = (r->recv_buf[1] & 0x80U) != 0U;

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
    case UDS_SID_READ_DATA_BY_IDENTIFIER:
    case UDS_SID_READ_MEMORY_BY_ADDRESS:
    case UDS_SID_WRITE_DATA_BY_IDENTIFIER:
    case UDS_SID_REQUEST_DOWNLOAD:
    case UDS_SID_REQUEST_UPLOAD:
    case UDS_SID_TRANSFER_DATA:
    case UDS_SID_REQUEST_FILE_TRANSFER:
    case UDS_SID_REQUEST_TRANSFER_EXIT: {
        UDS_ASSERT(service);
        response = service(srv, r);
        break;
    }

    /* CASE Service_optional */
    case UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION:
    case UDS_SID_READ_DTC_INFORMATION:
    case UDS_SID_READ_SCALING_DATA_BY_IDENTIFIER:
    case UDS_SID_READ_PERIODIC_DATA_BY_IDENTIFIER:
    case UDS_SID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
    case UDS_SID_IO_CONTROL_BY_IDENTIFIER:
    case UDS_SID_WRITE_MEMORY_BY_ADDRESS:
    case UDS_SID_ACCESS_TIMING_PARAMETER:
    case UDS_SID_SECURED_DATA_TRANSMISSION:
    case UDS_SID_RESPONSE_ON_EVENT:
    default: {
        if (service != NULL) {
            response = service(srv, r);
        } else {
            // this branch handles SIDs for which there is no registered handler.
            UDS_ASSERT(NULL == getServiceForSID(sid));

            // The standard says that the response ID (RID) is SID-0x40.
            // Both SID and RID are uint8_t, therefore no SID can be
            // greater than 0xFF-0x40 = 0xBF
            if (sid > 0xBFU) {
                return NegativeResponse(r, UDS_NRC_ServiceNotSupported);
            }

            UDSCustomArgs_t args = {
                .sid = sid,
                .optionRecord = &r->recv_buf[1],
                .len = (uint16_t)(r->recv_len - 1U),
                .copyResponse = safe_copy,
            };

            r->send_buf[0] = AsResponseSID(sid);
            r->send_len = 1;

            response = EmitEvent(srv, UDS_EVT_Custom, &args);
            if (UDS_PositiveResponse != response) {
                return NegativeResponse(r, response);
            }
            break;
        }
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

UDSErr_t UDSServerInit(UDSServer_t *srv) {
    if (NULL == srv) {
        return UDS_ERR_INVALID_ARG;
    }
    (void)memset(srv, 0, sizeof(UDSServer_t));
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
    if ((UDS_LEV_DS_DS != srv->sessionType) &&
        UDSTimeAfter(UDSMillis(), srv->s3_session_timeout_timer)) {
        (void)EmitEvent(srv, UDS_EVT_SessionTimeout, NULL);
        srv->sessionType = UDS_LEV_DS_DS;
        srv->securityLevel = 0;
    }

    if (srv->ecuResetScheduled && UDSTimeAfter(UDSMillis(), srv->ecuResetTimer)) {
        (void)EmitEvent(srv, UDS_EVT_DoScheduledReset, &srv->ecuResetScheduled);
    }

    {
        UDSErr_t err = UDSTpPoll(srv->tp);
        if (UDS_OK != err) {
            (void)EmitEvent(srv, UDS_EVT_Err, &err);
            UDS_LOGE(__FILE__, "UDSTpPoll failed with %s", UDSErrToStr(err));
        }
    }

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

            if (r->send_len > 0U) {
                UDSErr_t err = UDS_OK;
                err = UDSTpSend(srv->tp, r->send_buf, r->send_len, NULL);
                if (UDS_OK != err) {
                    (void)EmitEvent(srv, UDS_EVT_Err, &err);
                    UDS_LOGE(__FILE__, "UDSTpSend failed with %s", UDSErrToStr(err));
                }
            }

            if (srv->RCRRP) {
                // ISO14229-2:2013 Table 4 footnote b
                // min time between consecutive 0x78 responses is 0.3 * p2*
                uint32_t wait_time = srv->p2_star_ms * 3U / 10U;
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
        size_t tmp = 0;
        UDSErr_t err = UDSTpRecv(srv->tp, r->recv_buf, sizeof(r->recv_buf), &tmp, &r->info);
        if (UDS_OK != err) {
            UDS_LOGE(__FILE__, "UDSTpRecv failed with %s\n", UDSErrToStr(err));
            return;
        }
        r->recv_len = tmp;

        if (r->recv_len > 0U) {
            UDSErr_t response = evaluateServiceResponse(srv, r);
            srv->requestInProgress = true;
            if (UDS_NRC_RequestCorrectlyReceived_ResponsePending == response) {
                srv->RCRRP = true;
            }
        }
    }
}
