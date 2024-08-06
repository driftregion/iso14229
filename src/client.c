#include "client.h"
#include "config.h"
#include "util.h"

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
    printf("client state: %s (%d) -> %s (%d)\n", ClientStateName(client->state), client->state,
           ClientStateName(state), state);
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
            UDS_DBG_PRINT("received new timings: p2: %" PRIu16 ", p2*: %" PRIu32 "\n", p2, p2_star);
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
        UDSTpAddr_t ta_type = client->_options_copy & UDS_FUNCTIONAL ? UDS_A_TA_TYPE_FUNCTIONAL
                                                                     : UDS_A_TA_TYPE_PHYSICAL;
        UDSSDU_t info = {
            .A_Mtype = UDS_A_MTYPE_DIAG,
            .A_TA_Type = ta_type,
        };
        ssize_t ret = UDSTpSend(client->tp, client->send_buf, client->send_size, &info);
        if (ret < 0) {
            client->err = UDS_ERR_TPORT;
            UDS_DBG_PRINT("tport err: %zd\n", ret);
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
            printf("received %zd bytes\n", len);
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

UDSErr_t UDSSendRequestFileTransfer(UDSClient_t *client, enum FileOperationMode mode, const char *filePath, 
                                uint8_t dataFormatIdentifier, uint8_t fileSizeParameterLength, 
                                size_t fileSizeUncompressed, size_t fileSizeCompressed){
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    uint16_t filePathLen = strlen(filePath);
    if (filePathLen < 1)return UDS_ERR;

    uint8_t fileSizeBytes = 0;
    if ((mode == kAddFile) || (mode == kReplaceFile)){
        fileSizeBytes = fileSizeParameterLength;
    }
    size_t bufSize = 5 + filePathLen + fileSizeBytes + fileSizeBytes;
    if ((mode == kAddFile) || (mode == kReplaceFile) || (mode == kReadFile)){
        bufSize += 1;
    }
    if (client->send_buf_size < bufSize)return UDS_ERR_BUFSIZ;

    client->send_buf[0] = kSID_REQUEST_FILE_TRANSFER;
    client->send_buf[1] = mode;
    client->send_buf[2] = (filePathLen >> 8) & 0xFF;
    client->send_buf[3] = filePathLen & 0xFF;
    memcpy(&client->send_buf[4], filePath, filePathLen);
    if ((mode == kAddFile) || (mode == kReplaceFile) || (mode == kReadFile)){
        client->send_buf[4 + filePathLen] = dataFormatIdentifier;
    }
    if ((mode == kAddFile) || (mode == kReplaceFile)){
        client->send_buf[5 + filePathLen] = fileSizeParameterLength;
        uint8_t *ptr = &client->send_buf[6 + filePathLen];
        for (int i = fileSizeParameterLength - 1; i >= 0; i--) {
            *ptr = (fileSizeUncompressed & (0xFF << (8 * i))) >> (8 * i);
            ptr++;
        }

        for (int i = fileSizeParameterLength - 1; i >= 0; i--) {
            *ptr = (fileSizeCompressed & (0xFF << (8 * i))) >> (8 * i);
            ptr++;
        }
    }

    client->send_size = bufSize;
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
