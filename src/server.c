#include "server.h"
#include "config.h"
#include "uds.h"
#include "util.h"

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
    uintptr_t tmp = 0;
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

    if (!UDSTimeAfter(UDSMillis(), UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS)) {
        return NegativeResponse(r, kRequiredTimeDelayNotExpired);
    }

    if (!(UDSTimeAfter(UDSMillis(), srv->sec_access_auth_fail_timer))) {
        return NegativeResponse(r, kExceedNumberOfAttempts);
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
    if ((r->recv_len < UDS_0X3E_REQ_MIN_LEN) || (r->recv_len > UDS_0X3E_REQ_MAX_LEN)) {
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

    UDSTpPoll(srv->tp);

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
            printf("len: %zu\n", r->send_len);
            ssize_t ret = UDSTpSend(srv->tp, r->send_buf, r->send_len, NULL);
            // TODO test injection of transport errors:
            if (ret < 0) {
                UDSErr_t err = UDS_ERR_TPORT;
                EmitEvent(srv, UDS_SRV_EVT_Err, &err);
                UDS_DBG_PRINT("UDSTpSend failed with %zd\n", ret);
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
        if (r->recv_len > 0) {
            if (r->send_buf == NULL) {
                UDS_DBG_PRINT("Send buf null\n");
            }
            if (r->recv_buf == NULL) {
                UDS_DBG_PRINT("Recv buf null\n");
            }
            if (r->send_buf == NULL || r->recv_buf == NULL) {
                UDSErr_t err = UDS_ERR_TPORT;
                EmitEvent(srv, UDS_SRV_EVT_Err, &err);
                UDS_DBG_PRINT("bad tport\n");
                return;
            }
            uint8_t response = evaluateServiceResponse(srv, r);
            srv->requestInProgress = true;
            if (kRequestCorrectlyReceived_ResponsePending == response) {
                srv->RCRRP = true;
            }
        }
    }
}