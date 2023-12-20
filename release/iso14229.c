#include "iso14229.h"

#ifdef UDS_LINES
#line 1 "src/client.c"
#endif




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

#ifdef UDS_LINES
#line 1 "src/server.c"
#endif





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

    if (!UDSTimeAfter(UDSMillis(), srv->sec_access_boot_delay_timer)) {
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
    srv->sec_access_boot_delay_timer =
        UDSMillis() + UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS;
    srv->sec_access_auth_fail_timer = UDSMillis();
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
#ifdef UDS_LINES
#line 1 "src/tp.c"
#endif


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
    return hdl->send(hdl, (uint8_t *)buf, len, info);
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

#ifdef UDS_LINES
#line 1 "src/tp/isotp_c.c"
#endif
#if defined(UDS_TP_ISOTP_C)





static UDSTpStatus_t tp_poll(UDSTpHandle_t *hdl) {
    assert(hdl);
    UDSTpStatus_t status = 0;
    UDSISOTpC_t *impl = (UDSISOTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

int peek_link(IsoTpLink *link, uint8_t *buf, size_t bufsize, bool functional) {
    assert(link);
    assert(buf);
    int ret = -1;
    switch (link->receive_status) {
    case ISOTP_RECEIVE_STATUS_IDLE:
        ret = 0;
        goto done;
    case ISOTP_RECEIVE_STATUS_INPROGRESS:
        ret = 0;
        goto done;
    case ISOTP_RECEIVE_STATUS_FULL:
        ret = link->receive_size;
        printf("The link is full. Copying %d bytes\n", ret);
        memmove(buf, link->receive_buffer, link->receive_size);
        break;
    default:
        UDS_DBG_PRINT("receive_status %d not implemented\n", link->receive_status);
        ret = -1;
        goto done;
    }
done:
    return ret;
}

static ssize_t tp_peek(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    assert(hdl);
    assert(p_buf);
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    if (ISOTP_RECEIVE_STATUS_FULL == tp->phys_link.receive_status) { // recv not yet acked
        *p_buf = tp->recv_buf;
        return tp->phys_link.receive_size;
    }
    int ret = -1;
    ret = peek_link(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), false);
    UDS_A_TA_Type_t ta_type = UDS_A_TA_TYPE_PHYSICAL;
    uint32_t ta = tp->phys_ta;
    uint32_t sa = tp->phys_sa;

    if (ret > 0) {
        printf("just got %d bytes\n", ret);
        ta = tp->phys_sa;
        sa = tp->phys_ta;
        ta_type = UDS_A_TA_TYPE_PHYSICAL;
        *p_buf = tp->recv_buf;
        goto done;
    } else if (ret < 0) {
        goto done;
    } else {
        ret = peek_link(&tp->func_link, tp->recv_buf, sizeof(tp->recv_buf), true);
        if (ret > 0) {
            printf("just got %d bytes on func link \n", ret);
            ta = tp->func_sa;
            sa = tp->func_ta;
            ta_type = UDS_A_TA_TYPE_FUNCTIONAL;
            *p_buf = tp->recv_buf;
            goto done;
        } else if (ret < 0) {
            goto done;
        }
    }
done:
    if (ret > 0) {
        if (info) {
            info->A_TA = ta;
            info->A_SA = sa;
            info->A_TA_Type = ta_type;
        }
    }
    return ret;
}

static ssize_t tp_send(UDSTpHandle_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    assert(hdl);
    ssize_t ret = -1;
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;
    const uint32_t ta = ta_type == UDS_A_TA_TYPE_PHYSICAL ? tp->phys_ta : tp->func_ta;
    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &tp->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &tp->func_link;
        if (len > 7) {
            UDS_DBG_PRINT("Cannot send more than 7 bytes via functional addressing\n");
            ret = -3;
            goto done;
        }
        break;
    default:
        ret = -4;
        goto done;
    }

    int send_status = isotp_send(link, buf, len);
    switch (send_status) {
    case ISOTP_RET_OK:
        ret = len;
        goto done;
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        ret = send_status;
        goto done;
    }
done:
    return ret;
}

static void tp_ack_recv(UDSTpHandle_t *hdl) {
    assert(hdl);
    printf("ack recv\n");
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    uint16_t out_size = 0;
    isotp_receive(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), &out_size);
}

static ssize_t tp_get_send_buf(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    assert(hdl);
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    *p_buf = tp->send_buf;
    return sizeof(tp->send_buf);
}

UDSErr_t UDSISOTpCInit(UDSISOTpC_t *tp, const UDSISOTpCConfig_t *cfg) {
    if (cfg == NULL || tp == NULL) {
        return UDS_ERR;
    }
    tp->hdl.poll = tp_poll;
    tp->hdl.send = tp_send;
    tp->hdl.peek = tp_peek;
    tp->hdl.ack_recv = tp_ack_recv;
    tp->hdl.get_send_buf = tp_get_send_buf;
    tp->phys_sa = cfg->source_addr;
    tp->phys_ta = cfg->target_addr;
    tp->func_sa = cfg->source_addr_func;
    tp->func_ta = cfg->target_addr;

    isotp_init_link(&tp->phys_link, tp->phys_ta, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf), UDSMillis, cfg->isotp_user_send_can,
                    cfg->isotp_user_debug, cfg->user_data);
    isotp_init_link(&tp->func_link, tp->func_ta, tp->recv_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf), UDSMillis, cfg->isotp_user_send_can,
                    cfg->isotp_user_debug, cfg->user_data);
    return UDS_OK;
}

#endif

#ifdef UDS_LINES
#line 1 "src/tp/isotp_c_socketcan.c"
#endif
#if defined(UDS_TP_ISOTP_C_SOCKETCAN)




#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

static int SetupSocketCAN(const char *ifname) {
    UDS_DBG_PRINT("setting up CAN\n");
    struct sockaddr_can addr;
    struct ifreq ifr;
    int sockfd = -1;

    if ((sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("socket");
        goto done;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(sockfd, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
    }

done:
    return sockfd;
}

uint32_t isotp_user_get_ms(void) { return UDSMillis(); }

void isotp_user_debug(const char *message, ...) {
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    assert(user_data);
    int sockfd = *(int *)user_data;
    struct can_frame frame = {0};
    frame.can_id = arbitration_id;
    frame.can_dlc = size;
    memmove(frame.data, data, size);
    if (write(sockfd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write err");
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}

static void SocketCANRecv(UDSTpISOTpC_t *tp) {
    assert(tp);
    struct can_frame frame = {0};
    int nbytes = 0;

    for (;;) {
        nbytes = read(tp->fd, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                break;
            } else {
                perror("read");
            }
        } else if (nbytes == 0) {
            break;
        } else {
            if (frame.can_id == tp->phys_sa) {
                UDS_DBG_PRINT("phys recvd can\n");
                UDS_DBG_PRINTHEX(frame.data, frame.can_dlc);
                isotp_on_can_message(&tp->phys_link, frame.data, frame.can_dlc);
            } else if (frame.can_id == tp->func_sa) {
                if (ISOTP_RECEIVE_STATUS_IDLE != tp->phys_link.receive_status) {
                    UDS_DBG_PRINT(
                        "func frame received but cannot process because link is not idle");
                    return;
                }
                // TODO: reject if it's longer than a single frame
                isotp_on_can_message(&tp->func_link, frame.data, frame.can_dlc);
            }
        }
    }
}

static UDSTpStatus_t isotp_c_socketcan_tp_poll(UDSTpHandle_t *hdl) {
    assert(hdl);
    UDSTpStatus_t status = 0;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    SocketCANRecv(impl);
    isotp_poll(&impl->phys_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

static int isotp_c_socketcan_tp_peek_link(IsoTpLink *link, uint8_t *buf, size_t bufsize,
                                          bool functional) {
    assert(link);
    assert(buf);
    int ret = -1;
    switch (link->receive_status) {
    case ISOTP_RECEIVE_STATUS_IDLE:
        ret = 0;
        goto done;
    case ISOTP_RECEIVE_STATUS_INPROGRESS:
        ret = 0;
        goto done;
    case ISOTP_RECEIVE_STATUS_FULL:
        ret = link->receive_size;
        printf("The link is full. Copying %d bytes\n", ret);
        memmove(buf, link->receive_buffer, link->receive_size);
        break;
    default:
        UDS_DBG_PRINT("receive_status %d not implemented\n", link->receive_status);
        ret = -1;
        goto done;
    }
done:
    return ret;
}

static ssize_t isotp_c_socketcan_tp_peek(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    assert(hdl);
    assert(p_buf);
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    if (ISOTP_RECEIVE_STATUS_FULL == tp->phys_link.receive_status) { // recv not yet acked
        *p_buf = tp->recv_buf;
        return tp->phys_link.receive_size;
    }
    int ret = -1;
    ret = isotp_c_socketcan_tp_peek_link(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), false);
    UDS_A_TA_Type_t ta_type = UDS_A_TA_TYPE_PHYSICAL;
    uint32_t ta = tp->phys_ta;
    uint32_t sa = tp->phys_sa;

    if (ret > 0) {
        printf("just got %d bytes\n", ret);
        ta = tp->phys_sa;
        sa = tp->phys_ta;
        ta_type = UDS_A_TA_TYPE_PHYSICAL;
        *p_buf = tp->recv_buf;
        goto done;
    } else if (ret < 0) {
        goto done;
    } else {
        ret = isotp_c_socketcan_tp_peek_link(&tp->func_link, tp->recv_buf, sizeof(tp->recv_buf),
                                             true);
        if (ret > 0) {
            printf("just got %d bytes on func link \n", ret);
            ta = tp->func_sa;
            sa = tp->func_ta;
            ta_type = UDS_A_TA_TYPE_FUNCTIONAL;
            *p_buf = tp->recv_buf;
            goto done;
        } else if (ret < 0) {
            goto done;
        }
    }
done:
    if (ret > 0) {
        if (info) {
            info->A_TA = ta;
            info->A_SA = sa;
            info->A_TA_Type = ta_type;
        }
        fprintf(stdout, "%06d, %s recv, 0x%03x (%s), ", UDSMillis(), tp->tag, ta,
                ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        for (int i = 0; i < ret; i++) {
            fprintf(stdout, "%02x ", (*p_buf)[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout); // flush every time in case of crash
    }
    return ret;
}

static ssize_t isotp_c_socketcan_tp_send(UDSTpHandle_t *hdl, uint8_t *buf, size_t len,
                                         UDSSDU_t *info) {
    assert(hdl);
    ssize_t ret = -1;
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;
    const uint32_t ta = ta_type == UDS_A_TA_TYPE_PHYSICAL ? tp->phys_ta : tp->func_ta;
    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &tp->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &tp->func_link;
        if (len > 7) {
            UDS_DBG_PRINT("Cannot send more than 7 bytes via functional addressing\n");
            ret = -3;
            goto done;
        }
        break;
    default:
        ret = -4;
        goto done;
    }

    int send_status = isotp_send(link, buf, len);
    switch (send_status) {
    case ISOTP_RET_OK:
        ret = len;
        goto done;
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        ret = send_status;
        goto done;
    }
done:
    fprintf(stdout, "%06d, %s sends, 0x%03x (%s), ", UDSMillis(), tp->tag, ta,
            ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < len; i++) {
        fprintf(stdout, "%02x ", buf[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout); // flush every time in case of crash

    return ret;
}

static void isotp_c_socketcan_tp_ack_recv(UDSTpHandle_t *hdl) {
    assert(hdl);
    printf("ack recv\n");
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    uint16_t out_size = 0;
    isotp_receive(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), &out_size);
}

static ssize_t isotp_c_socketcan_tp_get_send_buf(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    assert(hdl);
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    *p_buf = tp->send_buf;
    return sizeof(tp->send_buf);
}

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, const char *ifname, uint32_t source_addr,
                         uint32_t target_addr, uint32_t source_addr_func,
                         uint32_t target_addr_func) {
    assert(tp);
    assert(ifname);
    tp->hdl.poll = isotp_c_socketcan_tp_poll;
    tp->hdl.send = isotp_c_socketcan_tp_send;
    tp->hdl.peek = isotp_c_socketcan_tp_peek;
    tp->hdl.ack_recv = isotp_c_socketcan_tp_ack_recv;
    tp->hdl.get_send_buf = isotp_c_socketcan_tp_get_send_buf;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;
    tp->func_ta = target_addr;
    tp->fd = SetupSocketCAN(ifname);

    isotp_init_link(&tp->phys_link, target_addr, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf), isotp_user_get_ms, isotp_user_send_can, isotp_user_debug,
                    &tp->fd);
    isotp_init_link(&tp->func_link, target_addr_func, tp->recv_buf, sizeof(tp->send_buf),
                    tp->recv_buf, sizeof(tp->recv_buf), isotp_user_get_ms, isotp_user_send_can,
                    isotp_user_debug, &tp->fd);
    return UDS_OK;
}

void UDSTpISOTpCDeinit(UDSTpISOTpC_t *tp) {
    assert(tp);
    close(tp->fd);
    tp->fd = -1;
}

#endif
#ifdef UDS_LINES
#line 1 "src/tp/isotp_sock.c"
#endif
#if defined(UDS_TP_ISOTP_SOCK)



#include <string.h>
#include <errno.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static UDSTpStatus_t isotp_sock_tp_poll(UDSTpHandle_t *hdl) { return 0; }

static ssize_t tp_recv_once(int fd, uint8_t *buf, size_t size) {
    ssize_t ret = read(fd, buf, size);
    if (ret < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            ret = 0;
        } else {
            UDS_DBG_PRINT("read failed: %ld with errno: %d\n", ret, errno);
            if (EILSEQ == errno) {
                UDS_DBG_PRINT("Perhaps I received multiple responses?\n");
            }
        }
    }
    return ret;
}

static ssize_t isotp_sock_tp_peek(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    assert(hdl);
    assert(p_buf);
    ssize_t ret = 0;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    *p_buf = impl->recv_buf;
    if (impl->recv_len) { // recv not yet acked
        ret = impl->recv_len;
        goto done;
    }

    UDSSDU_t *msg = &impl->recv_info;

    // recv acked, OK to receive
    ret = tp_recv_once(impl->phys_fd, impl->recv_buf, sizeof(impl->recv_buf));
    if (ret > 0) {
        msg->A_TA = impl->phys_sa;
        msg->A_SA = impl->phys_ta;
        msg->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
    } else {
        ret = tp_recv_once(impl->func_fd, impl->recv_buf, sizeof(impl->recv_buf));
        if (ret > 0) {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
            msg->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
        }
    }

    if (ret > 0) {
        fprintf(stdout, "%06d, %s recv, 0x%03x (%s), ", UDSMillis(), impl->tag, msg->A_TA,
                msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        for (unsigned i = 0; i < ret; i++) {
            fprintf(stdout, "%02x ", impl->recv_buf[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout); // flush every time in case of crash
        // UDS_DBG_PRINT("<<< ");
        // UDS_DBG_PRINTHEX(, ret);
    }

done:
    if (ret > 0) {
        impl->recv_len = ret;
        if (info) {
            *info = *msg;
        }
    }
    return ret;
}

static void isotp_sock_tp_ack_recv(UDSTpHandle_t *hdl) {
    assert(hdl);
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    impl->recv_len = 0;
}

static ssize_t isotp_sock_tp_send(UDSTpHandle_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    assert(hdl);
    ssize_t ret = -1;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    int fd;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        fd = impl->phys_fd;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {
        if (len > 7) {
            UDS_DBG_PRINT("UDSTpIsoTpSock: functional request too large\n");
            return -1;
        }
        fd = impl->func_fd;
    } else {
        ret = -4;
        goto done;
    }
    ret = write(fd, buf, len);
    if (ret < 0) {
        perror("write");
    }
done:
    // UDS_DBG_PRINT(">>> ");
    // UDS_DBG_PRINTHEX(buf, ret);

    fprintf(stdout, "%06d, %s sends, (%s), ", UDSMillis(), impl->tag,
            ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < len; i++) {
        fprintf(stdout, "%02x ", buf[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout); // flush every time in case of crash

    return ret;
}

static ssize_t isotp_sock_tp_get_send_buf(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    assert(hdl);
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    *p_buf = impl->send_buf;
    return sizeof(impl->send_buf);
}

static int LinuxSockBind(const char *if_name, uint32_t rxid, uint32_t txid, bool functional) {
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

    if (functional) {
        printf("configuring fd: %d as functional\n", fd);
        // configure the socket as listen-only to avoid sending FC frames
        struct can_isotp_options opts;
        memset(&opts, 0, sizeof(opts));
        opts.flags |= CAN_ISOTP_LISTEN_MODE;
        if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0) {
            perror("setsockopt (isotp_options):");
            return -1;
        }
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
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
    return fd;
}

UDSErr_t UDSTpIsoTpSockInitServer(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func) {
    assert(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.peek = isotp_sock_tp_peek;
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->hdl.ack_recv = isotp_sock_tp_ack_recv;
    tp->hdl.get_send_buf = isotp_sock_tp_get_send_buf;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, source_addr_func, 0, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        printf("foo\n");
        fflush(stdout);
        return UDS_ERR;
    }
    UDS_DBG_PRINT("%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x\n",
                  strlen(tp->tag) ? tp->tag : "server", source_addr, target_addr, source_addr_func,
                  target_addr);
    return UDS_OK;
}

UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func) {
    assert(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.peek = isotp_sock_tp_peek;
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->hdl.ack_recv = isotp_sock_tp_ack_recv;
    tp->hdl.get_send_buf = isotp_sock_tp_get_send_buf;
    tp->func_ta = target_addr_func;
    tp->phys_ta = target_addr;
    tp->phys_sa = source_addr;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, 0, target_addr_func, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        return UDS_ERR;
    }
    printf("%s initialized phys link (fd %d) rx 0x%03x tx 0x%03x func link (fd %d) rx 0x%03x tx "
           "0x%03x\n",
           strlen(tp->tag) ? tp->tag : "client", tp->phys_fd, source_addr, target_addr, tp->func_fd,
           source_addr, target_addr_func);
    return UDS_OK;
}

void UDSTpIsoTpSockDeinit(UDSTpIsoTpSock_t *tp) {
    if (tp) {
        if (close(tp->phys_fd) < 0) {
            perror("failed to close socket");
        }
        if (close(tp->func_fd) < 0) {
            perror("failed to close socket");
        }
    }
}

#endif

#ifdef UDS_LINES
#line 1 "src/tp/mock.c"
#endif
#if defined(UDS_TP_MOCK)



#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NUM_TP 16
#define NUM_MSGS 8
static TPMock_t *TPs[MAX_NUM_TP];
static unsigned TPCount = 0;
static FILE *LogFile = NULL;
static struct Msg {
    uint8_t buf[UDS_ISOTP_MTU];
    size_t len;
    UDSSDU_t info;
    uint32_t scheduled_tx_time;
} msgs[NUM_MSGS];
static unsigned MsgCount = 0;

static void LogMsg(const char *prefix, const uint8_t *buf, size_t len, UDSSDU_t *info) {
    if (!LogFile) {
        return;
    }
    fprintf(LogFile, "%06d, %s sends, 0x%03x (%s), ", UDSMillis(), prefix, info->A_TA,
            info->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < len; i++) {
        fprintf(LogFile, "%02x ", buf[i]);
    }
    fprintf(LogFile, "\n");
    fflush(LogFile); // flush every time in case of crash
}

static void NetworkPoll(void) {
    for (unsigned i = 0; i < MsgCount; i++) {
        struct Msg *msg = &msgs[i];
        if (UDSTimeAfter(UDSMillis(), msg->scheduled_tx_time)) {
            for (unsigned j = 0; j < TPCount; j++) {
                TPMock_t *tp = TPs[j];
                if (tp->sa_phys == msg->info.A_TA || tp->sa_func == msg->info.A_TA) {
                    if (tp->recv_len > 0) {
                        fprintf(stderr, "TPMock: %s recv buffer is already full. Message dropped\n",
                                tp->name);
                        continue;
                    }
                    memmove(tp->recv_buf, msg->buf, msg->len);
                    tp->recv_len = msg->len;
                    tp->recv_info = msg->info;
                }
            }
            LogMsg("network", msg->buf, msg->len, &msg->info);
            for (unsigned j = i + 1; j < MsgCount; j++) {
                msgs[j - 1] = msgs[j];
            }
            MsgCount--;
            i--;
        }
    }
}

static ssize_t mock_tp_peek(struct UDSTpHandle *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    assert(hdl);
    assert(p_buf);
    TPMock_t *tp = (TPMock_t *)hdl;
    if (p_buf) {
        *p_buf = tp->recv_buf;
    }
    if (info) {
        *info = tp->recv_info;
    }
    return tp->recv_len;
}

static ssize_t mock_tp_send(struct UDSTpHandle *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    assert(hdl);
    TPMock_t *tp = (TPMock_t *)hdl;
    if (MsgCount > NUM_MSGS) {
        fprintf(stderr, "TPMock: too many messages in the queue\n");
        return -1;
    }
    struct Msg *m = &msgs[MsgCount++];
    UDSTpAddr_t ta_type = info == NULL ? UDS_A_TA_TYPE_PHYSICAL : info->A_TA_Type;
    m->len = len;
    m->info.A_AE = info == NULL ? 0 : info->A_AE;
    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        m->info.A_TA = tp->ta_phys;
        m->info.A_SA = tp->sa_phys;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {
        m->info.A_TA = tp->ta_func;
        m->info.A_SA = tp->sa_func;
    } else {
        fprintf(stderr, "TPMock: unknown TA type: %d\n", ta_type);
        return -1;
    }
    m->info.A_TA_Type = ta_type;
    m->scheduled_tx_time = UDSMillis() + tp->send_tx_delay_ms;
    memmove(m->buf, buf, len);
    LogMsg(tp->name, buf, len, &m->info);
    return len;
}

static UDSTpStatus_t mock_tp_poll(struct UDSTpHandle *hdl) {
    NetworkPoll();
    // todo: make this status reflect TX time
    return UDS_TP_IDLE;
}

static ssize_t mock_tp_get_send_buf(struct UDSTpHandle *hdl, uint8_t **p_buf) {
    assert(hdl);
    assert(p_buf);
    TPMock_t *tp = (TPMock_t *)hdl;
    *p_buf = tp->send_buf;
    return sizeof(tp->send_buf);
}

static void mock_tp_ack_recv(struct UDSTpHandle *hdl) {
    assert(hdl);
    TPMock_t *tp = (TPMock_t *)hdl;
    tp->recv_len = 0;
}

static_assert(offsetof(TPMock_t, hdl) == 0, "TPMock_t must not have any members before hdl");

static void TPMockAttach(TPMock_t *tp, TPMockArgs_t *args) {
    assert(tp);
    assert(args);
    assert(TPCount < MAX_NUM_TP);
    TPs[TPCount++] = tp;
    tp->hdl.peek = mock_tp_peek;
    tp->hdl.send = mock_tp_send;
    tp->hdl.poll = mock_tp_poll;
    tp->hdl.get_send_buf = mock_tp_get_send_buf;
    tp->hdl.ack_recv = mock_tp_ack_recv;
    tp->sa_func = args->sa_func;
    tp->sa_phys = args->sa_phys;
    tp->ta_func = args->ta_func;
    tp->ta_phys = args->ta_phys;
    tp->recv_len = 0;
}

static void TPMockDetach(TPMock_t *tp) {
    assert(tp);
    for (unsigned i = 0; i < TPCount; i++) {
        if (TPs[i] == tp) {
            for (unsigned j = i + 1; j < TPCount; j++) {
                TPs[j - 1] = TPs[j];
            }
            TPCount--;
            printf("TPMock: detached %s. TPCount: %d\n", tp->name, TPCount);
            return;
        }
    }
    assert(false);
}

UDSTpHandle_t *TPMockNew(const char *name, TPMockArgs_t *args) {
    if (TPCount >= MAX_NUM_TP) {
        printf("TPCount: %d, too many TPs\n", TPCount);
        return NULL;
    }
    TPMock_t *tp = malloc(sizeof(TPMock_t));
    if (name) {
        strncpy(tp->name, name, sizeof(tp->name));
    } else {
        snprintf(tp->name, sizeof(tp->name), "TPMock%d", TPCount);
    }
    TPMockAttach(tp, args);
    return &tp->hdl;
}

void TPMockConnect(UDSTpHandle_t *tp1, UDSTpHandle_t *tp2);

void TPMockLogToFile(const char *filename) {
    if (LogFile) {
        fprintf(stderr, "Log file is already open\n");
        return;
    }
    if (!filename) {
        fprintf(stderr, "Filename is NULL\n");
        return;
    }
    // create file
    LogFile = fopen(filename, "w");
    if (!LogFile) {
        fprintf(stderr, "Failed to open log file %s\n", filename);
        return;
    }
}

void TPMockLogToStdout(void) {
    if (LogFile) {
        return;
    }
    LogFile = stdout;
}

void TPMockReset(void) {
    memset(TPs, 0, sizeof(TPs));
    TPCount = 0;
}

void TPMockFree(UDSTpHandle_t *tp) {
    TPMock_t *tpm = (TPMock_t *)tp;
    TPMockDetach(tpm);
    free(tp);
}

#endif

#ifdef UDS_LINES
#line 1 "src/util.c"
#endif


#if UDS_CUSTOM_MILLIS
#else
uint32_t UDSMillis(void) {
#if UDS_SYS == UDS_SYS_UNIX
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
#elif UDS_SYS == UDS_SYS_WINDOWS
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    long long milliseconds = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
    return milliseconds;
#elif UDS_SYS == UDS_SYS_ARDUINO
    return millis();
#elif UDS_SYS == UDS_SYS_ESP32
    return esp_timer_get_time() / 1000;
#else
#error "UDSMillis() undefined!"
#endif
}
#endif

bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel) {
    securityLevel &= 0x3f;
    return (0 == securityLevel || (0x43 <= securityLevel && securityLevel >= 0x5E) ||
            0x7F == securityLevel);
}

#ifdef UDS_LINES
#line 1 "bazel-out/k8-fastbuild/bin/isotp_c_wrapped.c"
#endif
#if defined(UDS_ISOTP_C)
#include <stdint.h>



///////////////////////////////////////////////////////
///                 STATIC FUNCTIONS                ///
///////////////////////////////////////////////////////

/* st_min to microsecond */
static uint8_t isotp_ms_to_st_min(uint8_t ms) {
    uint8_t st_min;

    st_min = ms;
    if (st_min > 0x7F) {
        st_min = 0x7F;
    }

    return st_min;
}

/* st_min to msec  */
static uint8_t isotp_st_min_to_ms(uint8_t st_min) {
    uint8_t ms;
    
    if (st_min >= 0xF1 && st_min <= 0xF9) {
        ms = 1;
    } else if (st_min <= 0x7F) {
        ms = st_min;
    } else {
        ms = 0;
    }

    return ms;
}

static int isotp_send_flow_control(IsoTpLink* link, uint8_t flow_status, uint8_t block_size, uint8_t st_min_ms) {

    IsoTpCanMessage message;
    int ret;

    /* setup message  */
    message.as.flow_control.type = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.FS = flow_status;
    message.as.flow_control.BS = block_size;
    message.as.flow_control.STmin = isotp_ms_to_st_min(st_min_ms);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void) memset(message.as.flow_control.reserve, 0, sizeof(message.as.flow_control.reserve));
    ret = link->isotp_user_send_can(link->send_arbitration_id, message.as.data_array.ptr, sizeof(message), link->user_data);
#else    
    ret = link->isotp_user_send_can(link->send_arbitration_id,
            message.as.data_array.ptr,
            3);
#endif

    return ret;
}

static int isotp_send_single_frame(IsoTpLink* link, uint32_t id) {

    IsoTpCanMessage message;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(link->send_size <= 7);

    /* setup message  */
    message.as.single_frame.type = ISOTP_PCI_TYPE_SINGLE;
    message.as.single_frame.SF_DL = (uint8_t) link->send_size;
    (void) memcpy(message.as.single_frame.data, link->send_buffer, link->send_size);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void) memset(message.as.single_frame.data + link->send_size, 0, sizeof(message.as.single_frame.data) - link->send_size);
    ret = link->isotp_user_send_can(id, message.as.data_array.ptr, sizeof(message), link->user_data);
#else
    ret = link->isotp_user_send_can(id,
            message.as.data_array.ptr,
            link->send_size + 1);
#endif

    return ret;
}

static int isotp_send_first_frame(IsoTpLink* link, uint32_t id) {
    
    IsoTpCanMessage message;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(link->send_size > 7);

    /* setup message  */
    message.as.first_frame.type = ISOTP_PCI_TYPE_FIRST_FRAME;
    message.as.first_frame.FF_DL_low = (uint8_t) link->send_size;
    message.as.first_frame.FF_DL_high = (uint8_t) (0x0F & (link->send_size >> 8));
    (void) memcpy(message.as.first_frame.data, link->send_buffer, sizeof(message.as.first_frame.data));

    /* send message */
    ret = link->isotp_user_send_can(id, message.as.data_array.ptr, sizeof(message), link->user_data);
    if (ISOTP_RET_OK == ret) {
        link->send_offset += sizeof(message.as.first_frame.data);
        link->send_sn = 1;
    }

    return ret;
}

static int isotp_send_consecutive_frame(IsoTpLink* link) {
    
    IsoTpCanMessage message;
    uint16_t data_length;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(link->send_size > 7);

    /* setup message  */
    message.as.consecutive_frame.type = TSOTP_PCI_TYPE_CONSECUTIVE_FRAME;
    message.as.consecutive_frame.SN = link->send_sn;
    data_length = link->send_size - link->send_offset;
    if (data_length > sizeof(message.as.consecutive_frame.data)) {
        data_length = sizeof(message.as.consecutive_frame.data);
    }
    (void) memcpy(message.as.consecutive_frame.data, link->send_buffer + link->send_offset, data_length);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void) memset(message.as.consecutive_frame.data + data_length, 0, sizeof(message.as.consecutive_frame.data) - data_length);
    ret = link->isotp_user_send_can(link->send_arbitration_id, message.as.data_array.ptr, sizeof(message), link->user_data);
#else
    ret = link->isotp_user_send_can(link->send_arbitration_id,
            message.as.data_array.ptr,
            data_length + 1);
#endif
    if (ISOTP_RET_OK == ret) {
        link->send_offset += data_length;
        if (++(link->send_sn) > 0x0F) {
            link->send_sn = 0;
        }
    }
    
    return ret;
}

static int isotp_receive_single_frame(IsoTpLink *link, IsoTpCanMessage *message, uint8_t len) {
    /* check data length */
    if ((0 == message->as.single_frame.SF_DL) || (message->as.single_frame.SF_DL > (len - 1))) {
        link->isotp_user_debug("Single-frame length too small.");
        return ISOTP_RET_LENGTH;
    }

    /* copying data */
    (void) memcpy(link->receive_buffer, message->as.single_frame.data, message->as.single_frame.SF_DL);
    link->receive_size = message->as.single_frame.SF_DL;
    
    return ISOTP_RET_OK;
}

static int isotp_receive_first_frame(IsoTpLink *link, IsoTpCanMessage *message, uint8_t len) {
    uint16_t payload_length;

    if (8 != len) {
        link->isotp_user_debug("First frame should be 8 bytes in length.");
        return ISOTP_RET_LENGTH;
    }

    /* check data length */
    payload_length = message->as.first_frame.FF_DL_high;
    payload_length = (payload_length << 8) + message->as.first_frame.FF_DL_low;

    /* should not use multiple frame transmition */
    if (payload_length <= 7) {
        link->isotp_user_debug("Should not use multiple frame transmission.");
        return ISOTP_RET_LENGTH;
    }
    
    if (payload_length > link->receive_buf_size) {
        link->isotp_user_debug("Multi-frame response too large for receiving buffer.");
        return ISOTP_RET_OVERFLOW;
    }
    
    /* copying data */
    (void) memcpy(link->receive_buffer, message->as.first_frame.data, sizeof(message->as.first_frame.data));
    link->receive_size = payload_length;
    link->receive_offset = sizeof(message->as.first_frame.data);
    link->receive_sn = 1;

    return ISOTP_RET_OK;
}

static int isotp_receive_consecutive_frame(IsoTpLink *link, IsoTpCanMessage *message, uint8_t len) {
    uint16_t remaining_bytes;
    
    /* check sn */
    if (link->receive_sn != message->as.consecutive_frame.SN) {
        return ISOTP_RET_WRONG_SN;
    }

    /* check data length */
    remaining_bytes = link->receive_size - link->receive_offset;
    if (remaining_bytes > sizeof(message->as.consecutive_frame.data)) {
        remaining_bytes = sizeof(message->as.consecutive_frame.data);
    }
    if (remaining_bytes > len - 1) {
        link->isotp_user_debug("Consecutive frame too short.");
        return ISOTP_RET_LENGTH;
    }

    /* copying data */
    (void) memcpy(link->receive_buffer + link->receive_offset, message->as.consecutive_frame.data, remaining_bytes);

    link->receive_offset += remaining_bytes;
    if (++(link->receive_sn) > 0x0F) {
        link->receive_sn = 0;
    }

    return ISOTP_RET_OK;
}

static int isotp_receive_flow_control_frame(IsoTpLink *link, IsoTpCanMessage *message, uint8_t len) {
    /* check message length */
    if (len < 3) {
        link->isotp_user_debug("Flow control frame too short.");
        return ISOTP_RET_LENGTH;
    }

    return ISOTP_RET_OK;
}

///////////////////////////////////////////////////////
///                 PUBLIC FUNCTIONS                ///
///////////////////////////////////////////////////////

int isotp_send(IsoTpLink *link, const uint8_t payload[], uint16_t size) {
    return isotp_send_with_id(link, link->send_arbitration_id, payload, size);
}

int isotp_send_with_id(IsoTpLink *link, uint32_t id, const uint8_t payload[], uint16_t size) {
    int ret;

    if (link == 0x0) {
        link->isotp_user_debug("Link is null!");
        return ISOTP_RET_ERROR;
    }

    if (size > link->send_buf_size) {
        link->isotp_user_debug("Message size too large. Increase ISO_TP_MAX_MESSAGE_SIZE to set a larger buffer\n");
        char message[128];
        sprintf(&message[0], "Attempted to send %d bytes; max size is %d!\n", size, link->send_buf_size);
        return ISOTP_RET_OVERFLOW;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status) {
        link->isotp_user_debug("Abort previous message, transmission in progress.\n");
        return ISOTP_RET_INPROGRESS;
    }

    /* copy into local buffer */
    link->send_size = size;
    link->send_offset = 0;
    (void) memcpy(link->send_buffer, payload, size);

    if (link->send_size < 8) {
        /* send single frame */
        ret = isotp_send_single_frame(link, id);
    } else {
        /* send multi-frame */
        ret = isotp_send_first_frame(link, id);

        /* init multi-frame control flags */
        if (ISOTP_RET_OK == ret) {
            link->send_bs_remain = 0;
            link->send_st_min = 0;
            link->send_wtf_count = 0;
            link->send_timer_st = link->isotp_user_get_ms();
            link->send_timer_bs = link->isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            link->send_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            link->send_status = ISOTP_SEND_STATUS_INPROGRESS;
        }
    }

    return ret;
}

void isotp_on_can_message(IsoTpLink *link, uint8_t *data, uint8_t len) {
    IsoTpCanMessage message;
    int ret;
    
    if (len < 2 || len > 8) {
        return;
    }

    memcpy(message.as.data_array.ptr, data, len);
    memset(message.as.data_array.ptr + len, 0, sizeof(message.as.data_array.ptr) - len);

    switch (message.as.common.type) {
        case ISOTP_PCI_TYPE_SINGLE: {
            /* update protocol result */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            } else {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }

            /* handle message */
            ret = isotp_receive_single_frame(link, &message, len);
            
            if (ISOTP_RET_OK == ret) {
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
            }
            break;
        }
        case ISOTP_PCI_TYPE_FIRST_FRAME: {
            /* update protocol result */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            } else {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }

            /* handle message */
            ret = isotp_receive_first_frame(link, &message, len);

            /* if overflow happened */
            if (ISOTP_RET_OVERFLOW == ret) {
                /* update protocol result */
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                /* send error message */
                isotp_send_flow_control(link, PCI_FLOW_STATUS_OVERFLOW, 0, 0);
                break;
            }

            /* if receive successful */
            if (ISOTP_RET_OK == ret) {
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_INPROGRESS;
                /* send fc frame */
                link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                /* refresh timer cs */
                link->receive_timer_cr = link->isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            }
            
            break;
        }
        case TSOTP_PCI_TYPE_CONSECUTIVE_FRAME: {
            /* check if in receiving status */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS != link->receive_status) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
                break;
            }

            /* handle message */
            ret = isotp_receive_consecutive_frame(link, &message, len);

            /* if wrong sn */
            if (ISOTP_RET_WRONG_SN == ret) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_WRONG_SN;
                link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                break;
            }

            /* if success */
            if (ISOTP_RET_OK == ret) {
                /* refresh timer cs */
                link->receive_timer_cr = link->isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
                
                /* receive finished */
                if (link->receive_offset >= link->receive_size) {
                    link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
                } else {
                    /* send fc when bs reaches limit */
                    if (0 == --link->receive_bs_count) {
                        link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                        isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                    }
                }
            }
            
            break;
        }
        case ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME:
            /* handle fc frame only when sending in progress  */
            if (ISOTP_SEND_STATUS_INPROGRESS != link->send_status) {
                break;
            }

            /* handle message */
            ret = isotp_receive_flow_control_frame(link, &message, len);
            
            if (ISOTP_RET_OK == ret) {
                /* refresh bs timer */
                link->send_timer_bs = link->isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

                /* overflow */
                if (PCI_FLOW_STATUS_OVERFLOW == message.as.flow_control.FS) {
                    link->send_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                    link->send_status = ISOTP_SEND_STATUS_ERROR;
                }

                /* wait */
                else if (PCI_FLOW_STATUS_WAIT == message.as.flow_control.FS) {
                    link->send_wtf_count += 1;
                    /* wait exceed allowed count */
                    if (link->send_wtf_count > ISO_TP_MAX_WFT_NUMBER) {
                        link->send_protocol_result = ISOTP_PROTOCOL_RESULT_WFT_OVRN;
                        link->send_status = ISOTP_SEND_STATUS_ERROR;
                    }
                }

                /* permit send */
                else if (PCI_FLOW_STATUS_CONTINUE == message.as.flow_control.FS) {
                    if (0 == message.as.flow_control.BS) {
                        link->send_bs_remain = ISOTP_INVALID_BS;
                    } else {
                        link->send_bs_remain = message.as.flow_control.BS;
                    }
                    link->send_st_min = isotp_st_min_to_ms(message.as.flow_control.STmin);
                    link->send_wtf_count = 0;
                }
            }
            break;
        default:
            break;
    };
    
    return;
}

int isotp_receive(IsoTpLink *link, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size) {
    uint16_t copylen;
    
    if (ISOTP_RECEIVE_STATUS_FULL != link->receive_status) {
        return ISOTP_RET_NO_DATA;
    }

    copylen = link->receive_size;
    if (copylen > payload_size) {
        copylen = payload_size;
    }

    memcpy(payload, link->receive_buffer, copylen);
    *out_size = copylen;

    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;

    return ISOTP_RET_OK;
}

void isotp_init_link(
    IsoTpLink *link,
    uint32_t sendid, 
    uint8_t *sendbuf, 
    uint16_t sendbufsize,
    uint8_t *recvbuf,
    uint16_t recvbufsize,
    uint32_t (*isotp_user_get_ms)(void),
    int (*isotp_user_send_can)(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size, void *user_data),
    void (*isotp_user_debug)(const char* message, ...),
    void *user_data
 ) {
    memset(link, 0, sizeof(*link));
    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    link->send_status = ISOTP_SEND_STATUS_IDLE;
    link->send_arbitration_id = sendid;
    link->send_buffer = sendbuf;
    link->send_buf_size = sendbufsize;
    link->receive_buffer = recvbuf;
    link->receive_buf_size = recvbufsize;
    link->isotp_user_get_ms = isotp_user_get_ms;
    link->isotp_user_send_can = isotp_user_send_can;
    link->isotp_user_debug = isotp_user_debug;
    link->user_data = user_data;

    return;
}

void isotp_poll(IsoTpLink *link) {
    int ret;

    /* only polling when operation in progress */
    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status) {

        /* continue send data */
        if (/* send data if bs_remain is invalid or bs_remain large than zero */
        (ISOTP_INVALID_BS == link->send_bs_remain || link->send_bs_remain > 0) &&
        /* and if st_min is zero or go beyond interval time */
        (0 == link->send_st_min || (0 != link->send_st_min && IsoTpTimeAfter(link->isotp_user_get_ms(), link->send_timer_st)))) {
            
            ret = isotp_send_consecutive_frame(link);
            if (ISOTP_RET_OK == ret) {
                if (ISOTP_INVALID_BS != link->send_bs_remain) {
                    link->send_bs_remain -= 1;
                }
                link->send_timer_bs = link->isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
                link->send_timer_st = link->isotp_user_get_ms() + link->send_st_min;

                /* check if send finish */
                if (link->send_offset >= link->send_size) {
                    link->send_status = ISOTP_SEND_STATUS_IDLE;
                }
            } else {
                link->send_status = ISOTP_SEND_STATUS_ERROR;
            }
        }

        /* check timeout */
        if (IsoTpTimeAfter(link->isotp_user_get_ms(), link->send_timer_bs)) {
            link->send_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_BS;
            link->send_status = ISOTP_SEND_STATUS_ERROR;
        }
    }

    /* only polling when operation in progress */
    if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {
        
        /* check timeout */
        if (IsoTpTimeAfter(link->isotp_user_get_ms(), link->receive_timer_cr)) {
            link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_CR;
            link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
        }
    }

    return;
}

#endif
