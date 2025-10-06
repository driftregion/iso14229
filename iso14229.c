/**
 * @file iso14229.c
 * @brief ISO14229-1 (UDS) library
 * @copyright Copyright (c) Nick Kirkby
 * @see https://github.com/driftregion/iso14229
 */

#include "iso14229.h"

#ifdef UDS_LINES
#line 1 "src/client.c"
#endif

// Client request states
#define STATE_IDLE 0
#define STATE_SENDING 1
#define STATE_AWAIT_SEND_COMPLETE 2
#define STATE_AWAIT_RESPONSE 3

UDSErr_t UDSClientInit(UDSClient_t *client) {
    if (NULL == client) {
        return UDS_ERR_INVALID_ARG;
    }
    memset(client, 0, sizeof(*client));
    client->state = STATE_IDLE;

    client->p2_ms = UDS_CLIENT_DEFAULT_P2_MS;
    client->p2_star_ms = UDS_CLIENT_DEFAULT_P2_STAR_MS;

    if (client->p2_star_ms < client->p2_ms) {
        UDS_LOGE(__FILE__, "p2_star_ms must be >= p2_ms");
        client->p2_star_ms = client->p2_ms;
    }

    return UDS_OK;
}

static const char *ClientStateName(uint8_t state) {
    switch (state) {
    case STATE_IDLE:
        return "Idle";
    case STATE_SENDING:
        return "Sending";
    case STATE_AWAIT_SEND_COMPLETE:
        return "AwaitSendComplete";
    case STATE_AWAIT_RESPONSE:
        return "AwaitResponse";
    default:
        return "Unknown";
    }
}

static void changeState(UDSClient_t *client, uint8_t state) {
    if (state != client->state) {
        UDS_LOGI(__FILE__, "client state: %s (%d) -> %s (%d)", ClientStateName(client->state),
                 client->state, ClientStateName(state), state);

        client->state = state;

        switch (state) {
        case STATE_IDLE:
            client->fn(client, UDS_EVT_Idle, NULL);
            break;
        case STATE_SENDING:
            break;
        case STATE_AWAIT_SEND_COMPLETE:
            break;
        case STATE_AWAIT_RESPONSE:
            break;
        default:
            UDS_ASSERT(0);
            break;
        }
    }
}

/**
 * @brief Check that the response is a valid UDS response
 * @param client
 * @return UDSErr_t
 */
static UDSErr_t ValidateServerResponse(const UDSClient_t *client) {

    if (client->recv_size < 1) {
        return UDS_ERR_RESP_TOO_SHORT;
    }

    if (0x7F == client->recv_buf[0]) { // Negative response
        if (client->recv_size < 2) {
            return UDS_ERR_RESP_TOO_SHORT;
        } else if (client->send_buf[0] != client->recv_buf[1]) {
            return UDS_ERR_SID_MISMATCH;
        } else if (UDS_NRC_RequestCorrectlyReceived_ResponsePending == client->recv_buf[2]) {
            return UDS_OK;
        } else {
            return client->recv_buf[2];
        }

    } else { // Positive response
        if (UDS_RESPONSE_SID_OF(client->send_buf[0]) != client->recv_buf[0]) {
            return UDS_ERR_SID_MISMATCH;
        }
        if (client->send_buf[0] == kSID_ECU_RESET) {
            if (client->recv_size < 2) {
                return UDS_ERR_RESP_TOO_SHORT;
            } else if (client->send_buf[1] != client->recv_buf[1]) {
                return UDS_ERR_SUBFUNCTION_MISMATCH;
            } else {
                ;
            }
        }
    }

    return UDS_OK;
}

/**
 * @brief Handle validated server response
 * @param client
 */
static UDSErr_t HandleServerResponse(UDSClient_t *client) {
    if (0x7F == client->recv_buf[0]) {
        if (UDS_NRC_RequestCorrectlyReceived_ResponsePending == client->recv_buf[2]) {
            client->p2_timer = UDSMillis() + client->p2_star_ms;
            UDS_LOGI(__FILE__, "got RCRRP, set p2 timer to %" PRIu32 "", client->p2_timer);
            memset(client->recv_buf, 0, sizeof(client->recv_buf));
            client->recv_size = 0;
            changeState(client, STATE_AWAIT_RESPONSE);
            return UDS_NRC_RequestCorrectlyReceived_ResponsePending;
        } else {
            ;
        }
    } else {
        uint8_t respSid = client->recv_buf[0];
        switch (UDS_REQUEST_SID_OF(respSid)) {
        case kSID_DIAGNOSTIC_SESSION_CONTROL: {
            if (client->recv_size < UDS_0X10_RESP_LEN) {
                UDS_LOGI(__FILE__, "Error: SID %x response too short",
                         kSID_DIAGNOSTIC_SESSION_CONTROL);
                changeState(client, STATE_IDLE);
                return UDS_ERR_RESP_TOO_SHORT;
            }

            if (client->_options_copy & UDS_IGNORE_SRV_TIMINGS) {
                changeState(client, STATE_IDLE);
                return UDS_OK;
            }

            uint16_t p2 =
                (uint16_t)(((uint16_t)client->recv_buf[2] << 8) | (uint16_t)client->recv_buf[3]);
            uint32_t p2_star = ((client->recv_buf[4] << 8) + client->recv_buf[5]) * 10;
            UDS_LOGI(__FILE__, "received new timings: p2: %" PRIu16 ", p2*: %" PRIu32, p2, p2_star);
            client->p2_ms = p2;
            client->p2_star_ms = p2_star;
            break;
        }
        default:
            break;
        }
    }
    return UDS_OK;
}

/**
 * @brief execute the client request state machine
 * @param client
 */
static UDSErr_t PollLowLevel(UDSClient_t *client) {
    UDSErr_t err = UDS_OK;
    UDS_ASSERT(client);

    if (NULL == client || NULL == client->tp || NULL == client->tp->poll) {
        return UDS_ERR_MISUSE;
    }

    UDSTpStatus_t tp_status = UDSTpPoll(client->tp);
    switch (client->state) {
    case STATE_IDLE: {
        client->options = client->defaultOptions;
        break;
    }
    case STATE_SENDING: {
        {
            UDSSDU_t info = {0};
            ssize_t len = UDSTpRecv(client->tp, client->recv_buf, sizeof(client->recv_buf), &info);
            if (len < 0) {
                UDS_LOGE(__FILE__, "transport returned error %zd", len);
            } else if (len == 0) {
                ; // expected
            } else {
                UDS_LOGW(__FILE__, "received %zd unexpected bytes:", len);
                UDS_LOG_SDU(__FILE__, client->recv_buf, len, &info);
            }
        }

        memset(client->recv_buf, 0, sizeof(client->recv_buf));
        client->recv_size = 0;

        UDSTpAddr_t ta_type = client->_options_copy & UDS_FUNCTIONAL ? UDS_A_TA_TYPE_FUNCTIONAL
                                                                     : UDS_A_TA_TYPE_PHYSICAL;
        UDSSDU_t info = {
            .A_Mtype = UDS_A_MTYPE_DIAG,
            .A_TA_Type = ta_type,
        };
        ssize_t ret = UDSTpSend(client->tp, client->send_buf, client->send_size, &info);
        if (ret < 0) {
            err = UDS_ERR_TPORT;
            UDS_LOGI(__FILE__, "tport err: %zd", ret);
        } else if (0 == ret) {
            UDS_LOGI(__FILE__, "send in progress...");
            ; // Waiting for send completion
        } else if (client->send_size == ret) {
            changeState(client, STATE_AWAIT_SEND_COMPLETE);
        } else {
            err = UDS_ERR_BUFSIZ;
        }
        break;
    }
    case STATE_AWAIT_SEND_COMPLETE: {
        if (client->_options_copy & UDS_FUNCTIONAL) {
            // "The Functional addressing is applied only to single frame transmission"
            // Specification of Diagnostic Communication (Diagnostic on CAN - Network Layer)
            changeState(client, STATE_IDLE);
        }
        if (tp_status & UDS_TP_SEND_IN_PROGRESS) {
            ; // await send complete
        } else {
            client->fn(client, UDS_EVT_SendComplete, NULL);
            if (client->_options_copy & UDS_SUPPRESS_POS_RESP) {
                changeState(client, STATE_IDLE);
            } else {
                changeState(client, STATE_AWAIT_RESPONSE);
                client->p2_timer = UDSMillis() + client->p2_ms;
            }
        }
        break;
    }
    case STATE_AWAIT_RESPONSE: {
        UDSSDU_t info = {0};

        ssize_t len = UDSTpRecv(client->tp, client->recv_buf, sizeof(client->recv_buf), &info);
        if (len < 0) {
            err = UDS_ERR_TPORT;
            changeState(client, STATE_IDLE);
        } else if (0 == len) {
            if (UDSTimeAfter(UDSMillis(), client->p2_timer)) {
                UDS_LOGI(__FILE__, "p2 timeout");
                err = UDS_ERR_TIMEOUT;
                changeState(client, STATE_IDLE);
            }
        } else {
            UDS_LOGI(__FILE__, "received %zd bytes. Processing...", len);
            UDS_ASSERT(len <= (ssize_t)UINT16_MAX);
            client->recv_size = (uint16_t)len;

            err = ValidateServerResponse(client);
            if (UDS_OK == err) {
                err = HandleServerResponse(client);
            }

            if (UDS_OK == err) {
                client->fn(client, UDS_EVT_ResponseReceived, NULL);
                changeState(client, STATE_IDLE);
            }
        }
        break;
    }

    default:
        UDS_ASSERT(0);
        break;
    }
    return err;
}

static UDSErr_t SendRequest(UDSClient_t *client) {
    client->_options_copy = client->options;

    if (client->_options_copy & UDS_SUPPRESS_POS_RESP) {
        // UDS-1:2013 8.2.2 Table 11
        client->send_buf[1] |= 0x80U;
    }

    changeState(client, STATE_SENDING);
    UDSErr_t err = PollLowLevel(client); // poll once to begin sending immediately
    return err;
}

static UDSErr_t PreRequestCheck(UDSClient_t *client) {
    if (NULL == client) {
        return UDS_ERR_INVALID_ARG;
    }
    if (STATE_IDLE != client->state) {
        return UDS_ERR_BUSY;
    }

    client->recv_size = 0;
    client->send_size = 0;

    if (client->tp == NULL) {
        return UDS_ERR_TPORT;
    }
    return UDS_OK;
}

UDSErr_t UDSSendBytes(UDSClient_t *client, const uint8_t *data, uint16_t size) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    if (size > sizeof(client->send_buf)) {
        return UDS_ERR_BUFSIZ;
    }
    memmove(client->send_buf, data, size);
    client->send_size = size;
    return SendRequest(client);
}

UDSErr_t UDSSendECUReset(UDSClient_t *client, uint8_t type) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    client->send_buf[0] = kSID_ECU_RESET;
    client->send_buf[1] = type;
    client->send_size = 2;
    return SendRequest(client);
}

UDSErr_t UDSSendDiagSessCtrl(UDSClient_t *client, uint8_t mode) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    client->send_buf[0] = kSID_DIAGNOSTIC_SESSION_CONTROL;
    client->send_buf[1] = mode;
    client->send_size = 2;
    return SendRequest(client);
}

UDSErr_t UDSSendCommCtrl(UDSClient_t *client, uint8_t ctrl, uint8_t comm) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    client->send_buf[0] = kSID_COMMUNICATION_CONTROL;
    client->send_buf[1] = ctrl;
    client->send_buf[2] = comm;
    client->send_size = 3;
    return SendRequest(client);
}

UDSErr_t UDSSendTesterPresent(UDSClient_t *client) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    client->send_buf[0] = kSID_TESTER_PRESENT;
    client->send_buf[1] = 0;
    client->send_size = 2;
    return SendRequest(client);
}

UDSErr_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                     const uint16_t numDataIdentifiers) {
    const uint16_t DID_LEN_BYTES = 2;
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    if (NULL == didList || 0 == numDataIdentifiers) {
        return UDS_ERR_INVALID_ARG;
    }
    client->send_buf[0] = kSID_READ_DATA_BY_IDENTIFIER;
    for (int i = 0; i < numDataIdentifiers; i++) {
        uint16_t offset = (uint16_t)(1 + DID_LEN_BYTES * i);
        if ((size_t)(offset + 2) > sizeof(client->send_buf)) {
            return UDS_ERR_INVALID_ARG;
        }
        (client->send_buf + offset)[0] = (didList[i] & 0xFF00) >> 8;
        (client->send_buf + offset)[1] = (didList[i] & 0xFF);
    }
    client->send_size = 1 + (numDataIdentifiers * DID_LEN_BYTES);
    return SendRequest(client);
}

UDSErr_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                     uint16_t size) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    if (data == NULL || size == 0) {
        return UDS_ERR_INVALID_ARG;
    }
    client->send_buf[0] = kSID_WRITE_DATA_BY_IDENTIFIER;
    if (sizeof(client->send_buf) <= 3 || size > sizeof(client->send_buf) - 3) {
        return UDS_ERR_BUFSIZ;
    }
    client->send_buf[1] = (dataIdentifier & 0xFF00) >> 8;
    client->send_buf[2] = (dataIdentifier & 0xFF);
    memmove(&client->send_buf[3], data, size);
    client->send_size = 3 + size;
    return SendRequest(client);
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
UDSErr_t UDSSendRoutineCtrl(UDSClient_t *client, uint8_t type, uint16_t routineIdentifier,
                            const uint8_t *data, uint16_t size) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    client->send_buf[0] = kSID_ROUTINE_CONTROL;
    client->send_buf[1] = type;
    client->send_buf[2] = routineIdentifier >> 8;
    client->send_buf[3] = routineIdentifier & 0xFF;
    if (size) {
        if (NULL == data) {
            return UDS_ERR_INVALID_ARG;
        }
        if (size > sizeof(client->send_buf) - UDS_0X31_REQ_MIN_LEN) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[UDS_0X31_REQ_MIN_LEN], data, size);
    } else {
        if (NULL != data) {
            UDS_LOGI(__FILE__, "warning: size zero and data non-null");
        }
    }
    client->send_size = UDS_0X31_REQ_MIN_LEN + size;
    return SendRequest(client);
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
        *ptr = (uint8_t)((memoryAddress >> (8 * i)) & 0xFF);
        ptr++;
    }

    for (int i = numMemorySizeBytes - 1; i >= 0; i--) {
        *ptr = (uint8_t)((memorySize >> (8 * i)) & 0xFF);
        ptr++;
    }

    client->send_size = UDS_0X34_REQ_BASE_LEN + numMemoryAddressBytes + numMemorySizeBytes;
    return SendRequest(client);
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
        *ptr = (uint8_t)((memoryAddress >> (8 * i)) & 0xFF);
        ptr++;
    }

    for (int i = numMemorySizeBytes - 1; i >= 0; i--) {
        *ptr = (uint8_t)((memorySize >> (8 * i)) & 0xFF);
        ptr++;
    }

    client->send_size = UDS_0X35_REQ_BASE_LEN + numMemoryAddressBytes + numMemorySizeBytes;
    return SendRequest(client);
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

    // blockLength must include SID and sequenceCounter
    if (blockLength <= 2) {
        return UDS_ERR_INVALID_ARG;
    }

    // data must fit inside blockLength - 2
    if (size > (blockLength - 2)) {
        return UDS_ERR_INVALID_ARG;
    }
    client->send_buf[0] = kSID_TRANSFER_DATA;
    client->send_buf[1] = blockSequenceCounter;
    memmove(&client->send_buf[UDS_0X36_REQ_BASE_LEN], data, size);
    UDS_LOGI(__FILE__, "size: %d, blocklength: %d", size, blockLength);
    client->send_size = UDS_0X36_REQ_BASE_LEN + size;
    return SendRequest(client);
}

UDSErr_t UDSSendTransferDataStream(UDSClient_t *client, uint8_t blockSequenceCounter,
                                   const uint16_t blockLength, FILE *fd) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    // blockLength must include SID and sequenceCounter
    if (blockLength <= 2) {
        return UDS_ERR_INVALID_ARG;
    }
    client->send_buf[0] = kSID_TRANSFER_DATA;
    client->send_buf[1] = blockSequenceCounter;

    size_t _size = fread(&client->send_buf[2], 1, blockLength - 2, fd);
    UDS_ASSERT(_size < UINT16_MAX);
    uint16_t size = (uint16_t)_size;
    UDS_LOGI(__FILE__, "size: %d, blocklength: %d", size, blockLength);
    client->send_size = UDS_0X36_REQ_BASE_LEN + size;
    return SendRequest(client);
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
    return SendRequest(client);
}

UDSErr_t UDSSendRequestFileTransfer(UDSClient_t *client, uint8_t mode, const char *filePath,
                                    uint8_t dataFormatIdentifier, uint8_t fileSizeParameterLength,
                                    size_t fileSizeUncompressed, size_t fileSizeCompressed) {
    UDSErr_t err = PreRequestCheck(client);
    if (err) {
        return err;
    }
    if (filePath == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    size_t filePathLenSize = strnlen(filePath, UINT16_MAX + 1);
    if (filePathLenSize == 0) {
        return UDS_ERR_INVALID_ARG;
    }
    if (filePathLenSize > UINT16_MAX) {
        return UDS_ERR_INVALID_ARG;
    }
    uint16_t filePathLen = (uint16_t)filePathLenSize;

    uint8_t fileSizeBytes = 0;
    if ((mode == UDS_MOOP_ADDFILE) || (mode == UDS_MOOP_REPLFILE)) {
        fileSizeBytes = fileSizeParameterLength;
    }
    size_t bufSize = 5 + filePathLen + fileSizeBytes + fileSizeBytes;
    if ((mode == UDS_MOOP_ADDFILE) || (mode == UDS_MOOP_REPLFILE) || (mode == UDS_MOOP_RDFILE)) {
        bufSize += 1;
    }
    if (sizeof(client->send_buf) < bufSize)
        return UDS_ERR_BUFSIZ;

    client->send_buf[0] = kSID_REQUEST_FILE_TRANSFER;
    client->send_buf[1] = mode;
    client->send_buf[2] = (filePathLen >> 8) & 0xFF;
    client->send_buf[3] = filePathLen & 0xFF;
    if (filePathLen > sizeof(client->send_buf) - 4)
        return UDS_ERR_BUFSIZ;
    memcpy(&client->send_buf[4], filePath, filePathLen);
    if ((mode == UDS_MOOP_ADDFILE) || (mode == UDS_MOOP_REPLFILE) || (mode == UDS_MOOP_RDFILE)) {
        client->send_buf[4 + filePathLen] = dataFormatIdentifier;
    }
    if ((mode == UDS_MOOP_ADDFILE) || (mode == UDS_MOOP_REPLFILE)) {
        client->send_buf[5 + filePathLen] = fileSizeParameterLength;
        uint8_t *ptr = &client->send_buf[6 + filePathLen];
        for (int i = fileSizeParameterLength - 1; i >= 0; i--) {
            *ptr = (uint8_t)((fileSizeUncompressed >> (8 * i)) & 0xFF);
            ptr++;
        }

        for (int i = fileSizeParameterLength - 1; i >= 0; i--) {
            *ptr = (uint8_t)((fileSizeCompressed >> (8 * i)) & 0xFF);
            ptr++;
        }
    }

    client->send_size = (uint16_t)bufSize;
    return SendRequest(client);
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

    // these are reserved values
    if (0x00 == dtcSettingType || 0x7F == dtcSettingType ||
        (0x03 <= dtcSettingType && dtcSettingType <= 0x3F)) {
        return UDS_ERR_INVALID_ARG;
    }

    client->send_buf[0] = kSID_CONTROL_DTC_SETTING;
    client->send_buf[1] = dtcSettingType;

    if (NULL == data) {
        if (size != 0) {
            return UDS_ERR_INVALID_ARG;
        }
    } else {
        if (size == 0) {
            UDS_LOGI(__FILE__, "warning: size == 0 and data is non-null");
        }
        if (size > sizeof(client->send_buf) - 2) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[2], data, size);
    }
    client->send_size = 2 + size;
    return SendRequest(client);
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

    if (size > sizeof(client->send_buf) - UDS_0X27_REQ_BASE_LEN) {
        return UDS_ERR_BUFSIZ;
    }
    if (size == 0 && NULL != data) {
        UDS_LOGE(__FILE__, "size == 0 and data is non-null");
        return UDS_ERR_INVALID_ARG;
    }
    if (size > 0 && NULL == data) {
        UDS_LOGE(__FILE__, "size > 0 but data is null");
        return UDS_ERR_INVALID_ARG;
    }
    if (size > 0) {
        memmove(&client->send_buf[UDS_0X27_REQ_BASE_LEN], data, size);
    }

    client->send_size = UDS_0X27_REQ_BASE_LEN + size;
    return SendRequest(client);
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
    if (NULL == client || NULL == resp) {
        return UDS_ERR_INVALID_ARG;
    }
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
    if (NULL == client || NULL == resp) {
        return UDS_ERR_INVALID_ARG;
    }
    if (UDS_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL) != client->recv_buf[0]) {
        return UDS_ERR_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X31_RESP_MIN_LEN) {
        return UDS_ERR_RESP_TOO_SHORT;
    }
    resp->routineControlType = client->recv_buf[1];
    resp->routineIdentifier =
        (uint16_t)((uint16_t)(client->recv_buf[2] << 8) | (uint16_t)client->recv_buf[3]);
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
    if (NULL == client || NULL == resp) {
        return UDS_ERR_INVALID_ARG;
    }
    if (UDS_RESPONSE_SID_OF(kSID_REQUEST_DOWNLOAD) != client->recv_buf[0]) {
        return UDS_ERR_SID_MISMATCH;
    }
    if (client->recv_size < UDS_0X34_RESP_BASE_LEN) {
        return UDS_ERR_RESP_TOO_SHORT;
    }
    uint8_t maxNumberOfBlockLengthSize = (client->recv_buf[1] & 0xF0) >> 4;

    if (sizeof(resp->maxNumberOfBlockLength) < maxNumberOfBlockLengthSize) {
        UDS_LOGI(__FILE__, "WARNING: sizeof(maxNumberOfBlockLength) > sizeof(size_t)");
        return UDS_FAIL;
    }
    resp->maxNumberOfBlockLength = 0;
    for (uint8_t byteIdx = 0; byteIdx < maxNumberOfBlockLengthSize; byteIdx++) {
        uint8_t byte = client->recv_buf[UDS_0X34_RESP_BASE_LEN + byteIdx];
        uint8_t shiftBytes = maxNumberOfBlockLengthSize - 1 - byteIdx;
        resp->maxNumberOfBlockLength |= byte << (8 * shiftBytes);
    }
    return UDS_OK;
}

UDSErr_t UDSClientPoll(UDSClient_t *client) {
    if (NULL == client->fn) {
        return UDS_ERR_MISUSE;
    }

    UDSErr_t err = PollLowLevel(client);

    if (err == UDS_OK || err == UDS_NRC_RequestCorrectlyReceived_ResponsePending) {
        ;
    } else {
        client->fn(client, UDS_EVT_Err, &err);
        changeState(client, STATE_IDLE);
    }

    client->fn(client, UDS_EVT_Poll, NULL);
    return err;
}

UDSErr_t UDSUnpackRDBIResponse(UDSClient_t *client, UDSRDBIVar_t *vars, uint16_t numVars) {
    uint16_t offset = UDS_0X22_RESP_BASE_LEN;
    if (client == NULL || vars == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    for (int i = 0; i < numVars; i++) {

        if (offset + sizeof(uint16_t) > client->recv_size) {
            return UDS_ERR_RESP_TOO_SHORT;
        }
        uint16_t did = (uint16_t)((uint16_t)(client->recv_buf[offset] << 8) |
                                  (uint16_t)client->recv_buf[offset + 1]);
        if (did != vars[i].did) {
            return UDS_ERR_DID_MISMATCH;
        }
        if (offset + sizeof(uint16_t) + vars[i].len > client->recv_size) {
            return UDS_ERR_RESP_TOO_SHORT;
        }
        if (vars[i].UnpackFn) {
            vars[i].UnpackFn(vars[i].data, client->recv_buf + offset + sizeof(uint16_t),
                             vars[i].len);
        } else {
            return UDS_ERR_INVALID_ARG;
        }
        offset += sizeof(uint16_t) + vars[i].len;
    }
    return UDS_OK;
}


#ifdef UDS_LINES
#line 1 "src/server.c"
#endif
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


#ifdef UDS_LINES
#line 1 "src/tp.c"
#endif

ssize_t UDSTpSend(struct UDSTp *hdl, const uint8_t *buf, ssize_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->send);
    return hdl->send(hdl, (uint8_t *)buf, len, info);
}

ssize_t UDSTpRecv(struct UDSTp *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->recv);
    return hdl->recv(hdl, buf, bufsize, info);
}

UDSTpStatus_t UDSTpPoll(struct UDSTp *hdl) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->poll);
    return hdl->poll(hdl);
}

#ifdef UDS_LINES
#line 1 "src/util.c"
#endif

#if UDS_CUSTOM_MILLIS
#else
uint32_t UDSMillis(void) {
#if UDS_SYS == UDS_SYS_UNIX
    struct timeval te;
    gettimeofday(&te, NULL); // cppcheck-suppress misra-c2012-21.6
    long long milliseconds = (te.tv_sec * 1000LL) + (te.tv_usec / 1000);
    return (uint32_t)milliseconds;
#elif UDS_SYS == UDS_SYS_WINDOWS
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    long long milliseconds = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
    return (uint32_t)milliseconds;
#elif UDS_SYS == UDS_SYS_ARDUINO
    return millis();
#elif UDS_SYS == UDS_SYS_ESP32
    return esp_timer_get_time() / 1000;
#else
#error "UDSMillis() undefined!"
#endif
}
#endif

/**
 * @brief Check if a security level is reserved per ISO14229-1:2020 Table 42
 *
 * @param securityLevel
 * @return true
 * @return false
 */
bool UDSSecurityAccessLevelIsReserved(uint8_t subFunction) {
    uint8_t securityLevel = subFunction & 0x3F;
    if (0u == securityLevel) {
        return true;
    }
    if ((securityLevel >= 0x43u) && (securityLevel <= 0x5Eu)) {
        return true;
    }
    if (securityLevel == 0x7Fu) {
        return true;
    }
    return false;
}

const char *UDSErrToStr(UDSErr_t err) {
    switch (err) {
    case UDS_OK:
        return "UDS_OK";
    case UDS_FAIL:
        return "UDS_FAIL";
    case UDS_NRC_GeneralReject:
        return "UDS_NRC_GeneralReject";
    case UDS_NRC_ServiceNotSupported:
        return "UDS_NRC_ServiceNotSupported";
    case UDS_NRC_SubFunctionNotSupported:
        return "UDS_NRC_SubFunctionNotSupported";
    case UDS_NRC_IncorrectMessageLengthOrInvalidFormat:
        return "UDS_NRC_IncorrectMessageLengthOrInvalidFormat";
    case UDS_NRC_ResponseTooLong:
        return "UDS_NRC_ResponseTooLong";
    case UDS_NRC_BusyRepeatRequest:
        return "UDS_NRC_BusyRepeatRequest";
    case UDS_NRC_ConditionsNotCorrect:
        return "UDS_NRC_ConditionsNotCorrect";
    case UDS_NRC_RequestSequenceError:
        return "UDS_NRC_RequestSequenceError";
    case UDS_NRC_NoResponseFromSubnetComponent:
        return "UDS_NRC_NoResponseFromSubnetComponent";
    case UDS_NRC_FailurePreventsExecutionOfRequestedAction:
        return "UDS_NRC_FailurePreventsExecutionOfRequestedAction";
    case UDS_NRC_RequestOutOfRange:
        return "UDS_NRC_RequestOutOfRange";
    case UDS_NRC_SecurityAccessDenied:
        return "UDS_NRC_SecurityAccessDenied";
    case UDS_NRC_AuthenticationRequired:
        return "UDS_NRC_AuthenticationRequired";
    case UDS_NRC_InvalidKey:
        return "UDS_NRC_InvalidKey";
    case UDS_NRC_ExceedNumberOfAttempts:
        return "UDS_NRC_ExceedNumberOfAttempts";
    case UDS_NRC_RequiredTimeDelayNotExpired:
        return "UDS_NRC_RequiredTimeDelayNotExpired";
    case UDS_NRC_SecureDataTransmissionRequired:
        return "UDS_NRC_SecureDataTransmissionRequired";
    case UDS_NRC_SecureDataTransmissionNotAllowed:
        return "UDS_NRC_SecureDataTransmissionNotAllowed";
    case UDS_NRC_SecureDataVerificationFailed:
        return "UDS_NRC_SecureDataVerificationFailed";
    case UDS_NRC_CertficateVerificationFailedInvalidTimePeriod:
        return "UDS_NRC_CertficateVerificationFailedInvalidTimePeriod";
    case UDS_NRC_CertficateVerificationFailedInvalidSignature:
        return "UDS_NRC_CertficateVerificationFailedInvalidSignature";
    case UDS_NRC_CertficateVerificationFailedInvalidChainOfTrust:
        return "UDS_NRC_CertficateVerificationFailedInvalidChainOfTrust";
    case UDS_NRC_CertficateVerificationFailedInvalidType:
        return "UDS_NRC_CertficateVerificationFailedInvalidType";
    case UDS_NRC_CertficateVerificationFailedInvalidFormat:
        return "UDS_NRC_CertficateVerificationFailedInvalidFormat";
    case UDS_NRC_CertficateVerificationFailedInvalidContent:
        return "UDS_NRC_CertficateVerificationFailedInvalidContent";
    case UDS_NRC_CertficateVerificationFailedInvalidScope:
        return "UDS_NRC_CertficateVerificationFailedInvalidScope";
    case UDS_NRC_CertficateVerificationFailedInvalidCertificate:
        return "UDS_NRC_CertficateVerificationFailedInvalidCertificate";
    case UDS_NRC_OwnershipVerificationFailed:
        return "UDS_NRC_OwnershipVerificationFailed";
    case UDS_NRC_ChallengeCalculationFailed:
        return "UDS_NRC_ChallengeCalculationFailed";
    case UDS_NRC_SettingAccessRightsFailed:
        return "UDS_NRC_SettingAccessRightsFailed";
    case UDS_NRC_SessionKeyCreationOrDerivationFailed:
        return "UDS_NRC_SessionKeyCreationOrDerivationFailed";
    case UDS_NRC_ConfigurationDataUsageFailed:
        return "UDS_NRC_ConfigurationDataUsageFailed";
    case UDS_NRC_DeAuthenticationFailed:
        return "UDS_NRC_DeAuthenticationFailed";
    case UDS_NRC_UploadDownloadNotAccepted:
        return "UDS_NRC_UploadDownloadNotAccepted";
    case UDS_NRC_TransferDataSuspended:
        return "UDS_NRC_TransferDataSuspended";
    case UDS_NRC_GeneralProgrammingFailure:
        return "UDS_NRC_GeneralProgrammingFailure";
    case UDS_NRC_WrongBlockSequenceCounter:
        return "UDS_NRC_WrongBlockSequenceCounter";
    case UDS_NRC_RequestCorrectlyReceived_ResponsePending:
        return "UDS_NRC_RequestCorrectlyReceived_ResponsePending";
    case UDS_NRC_SubFunctionNotSupportedInActiveSession:
        return "UDS_NRC_SubFunctionNotSupportedInActiveSession";
    case UDS_NRC_ServiceNotSupportedInActiveSession:
        return "UDS_NRC_ServiceNotSupportedInActiveSession";
    case UDS_NRC_RpmTooHigh:
        return "UDS_NRC_RpmTooHigh";
    case UDS_NRC_RpmTooLow:
        return "UDS_NRC_RpmTooLow";
    case UDS_NRC_EngineIsRunning:
        return "UDS_NRC_EngineIsRunning";
    case UDS_NRC_EngineIsNotRunning:
        return "UDS_NRC_EngineIsNotRunning";
    case UDS_NRC_EngineRunTimeTooLow:
        return "UDS_NRC_EngineRunTimeTooLow";
    case UDS_NRC_TemperatureTooHigh:
        return "UDS_NRC_TemperatureTooHigh";
    case UDS_NRC_TemperatureTooLow:
        return "UDS_NRC_TemperatureTooLow";
    case UDS_NRC_VehicleSpeedTooHigh:
        return "UDS_NRC_VehicleSpeedTooHigh";
    case UDS_NRC_VehicleSpeedTooLow:
        return "UDS_NRC_VehicleSpeedTooLow";
    case UDS_NRC_ThrottlePedalTooHigh:
        return "UDS_NRC_ThrottlePedalTooHigh";
    case UDS_NRC_ThrottlePedalTooLow:
        return "UDS_NRC_ThrottlePedalTooLow";
    case UDS_NRC_TransmissionRangeNotInNeutral:
        return "UDS_NRC_TransmissionRangeNotInNeutral";
    case UDS_NRC_TransmissionRangeNotInGear:
        return "UDS_NRC_TransmissionRangeNotInGear";
    case UDS_NRC_BrakeSwitchNotClosed:
        return "UDS_NRC_BrakeSwitchNotClosed";
    case UDS_NRC_ShifterLeverNotInPark:
        return "UDS_NRC_ShifterLeverNotInPark";
    case UDS_NRC_TorqueConverterClutchLocked:
        return "UDS_NRC_TorqueConverterClutchLocked";
    case UDS_NRC_VoltageTooHigh:
        return "UDS_NRC_VoltageTooHigh";
    case UDS_NRC_VoltageTooLow:
        return "UDS_NRC_VoltageTooLow";
    case UDS_NRC_ResourceTemporarilyNotAvailable:
        return "UDS_NRC_ResourceTemporarilyNotAvailable";
    case UDS_ERR_TIMEOUT:
        return "UDS_ERR_TIMEOUT";
    case UDS_ERR_DID_MISMATCH:
        return "UDS_ERR_DID_MISMATCH";
    case UDS_ERR_SID_MISMATCH:
        return "UDS_ERR_SID_MISMATCH";
    case UDS_ERR_SUBFUNCTION_MISMATCH:
        return "UDS_ERR_SUBFUNCTION_MISMATCH";
    case UDS_ERR_TPORT:
        return "UDS_ERR_TPORT";
    case UDS_ERR_RESP_TOO_SHORT:
        return "UDS_ERR_RESP_TOO_SHORT";
    case UDS_ERR_BUFSIZ:
        return "UDS_ERR_BUFSIZ";
    case UDS_ERR_INVALID_ARG:
        return "UDS_ERR_INVALID_ARG";
    case UDS_ERR_BUSY:
        return "UDS_ERR_BUSY";
    case UDS_ERR_MISUSE:
        return "UDS_ERR_MISUSE";
    default:
        return "unknown";
    }
}

const char *UDSEventToStr(UDSEvent_t evt) {

    switch (evt) {
    case UDS_EVT_Custom:
        return "UDS_EVT_Custom";
    case UDS_EVT_Err:
        return "UDS_EVT_Err";
    case UDS_EVT_DiagSessCtrl:
        return "UDS_EVT_DiagSessCtrl";
    case UDS_EVT_EcuReset:
        return "UDS_EVT_EcuReset";
    case UDS_EVT_ReadDataByIdent:
        return "UDS_EVT_ReadDataByIdent";
    case UDS_EVT_ReadMemByAddr:
        return "UDS_EVT_ReadMemByAddr";
    case UDS_EVT_CommCtrl:
        return "UDS_EVT_CommCtrl";
    case UDS_EVT_SecAccessRequestSeed:
        return "UDS_EVT_SecAccessRequestSeed";
    case UDS_EVT_SecAccessValidateKey:
        return "UDS_EVT_SecAccessValidateKey";
    case UDS_EVT_WriteDataByIdent:
        return "UDS_EVT_WriteDataByIdent";
    case UDS_EVT_RoutineCtrl:
        return "UDS_EVT_RoutineCtrl";
    case UDS_EVT_RequestDownload:
        return "UDS_EVT_RequestDownload";
    case UDS_EVT_RequestUpload:
        return "UDS_EVT_RequestUpload";
    case UDS_EVT_TransferData:
        return "UDS_EVT_TransferData";
    case UDS_EVT_RequestTransferExit:
        return "UDS_EVT_RequestTransferExit";
    case UDS_EVT_SessionTimeout:
        return "UDS_EVT_SessionTimeout";
    case UDS_EVT_DoScheduledReset:
        return "UDS_EVT_DoScheduledReset";
    case UDS_EVT_RequestFileTransfer:
        return "UDS_EVT_RequestFileTransfer";
    case UDS_EVT_Poll:
        return "UDS_EVT_Poll";
    case UDS_EVT_SendComplete:
        return "UDS_EVT_SendComplete";
    case UDS_EVT_ResponseReceived:
        return "UDS_EVT_ResponseReceived";
    case UDS_EVT_Idle:
        return "UDS_EVT_Idle";
    case UDS_EVT_MAX:
        return "UDS_EVT_MAX";
    default:
        return "unknown";
    }
}

bool UDSErrIsNRC(UDSErr_t err) {
    switch (err) {
    case UDS_PositiveResponse:
    case UDS_NRC_GeneralReject:
    case UDS_NRC_ServiceNotSupported:
    case UDS_NRC_SubFunctionNotSupported:
    case UDS_NRC_IncorrectMessageLengthOrInvalidFormat:
    case UDS_NRC_ResponseTooLong:
    case UDS_NRC_BusyRepeatRequest:
    case UDS_NRC_ConditionsNotCorrect:
    case UDS_NRC_RequestSequenceError:
    case UDS_NRC_NoResponseFromSubnetComponent:
    case UDS_NRC_FailurePreventsExecutionOfRequestedAction:
    case UDS_NRC_RequestOutOfRange:
    case UDS_NRC_SecurityAccessDenied:
    case UDS_NRC_AuthenticationRequired:
    case UDS_NRC_InvalidKey:
    case UDS_NRC_ExceedNumberOfAttempts:
    case UDS_NRC_RequiredTimeDelayNotExpired:
    case UDS_NRC_SecureDataTransmissionRequired:
    case UDS_NRC_SecureDataTransmissionNotAllowed:
    case UDS_NRC_SecureDataVerificationFailed:
    case UDS_NRC_CertficateVerificationFailedInvalidTimePeriod:
    case UDS_NRC_CertficateVerificationFailedInvalidSignature:
    case UDS_NRC_CertficateVerificationFailedInvalidChainOfTrust:
    case UDS_NRC_CertficateVerificationFailedInvalidType:
    case UDS_NRC_CertficateVerificationFailedInvalidFormat:
    case UDS_NRC_CertficateVerificationFailedInvalidContent:
    case UDS_NRC_CertficateVerificationFailedInvalidScope:
    case UDS_NRC_CertficateVerificationFailedInvalidCertificate:
    case UDS_NRC_OwnershipVerificationFailed:
    case UDS_NRC_ChallengeCalculationFailed:
    case UDS_NRC_SettingAccessRightsFailed:
    case UDS_NRC_SessionKeyCreationOrDerivationFailed:
    case UDS_NRC_ConfigurationDataUsageFailed:
    case UDS_NRC_DeAuthenticationFailed:
    case UDS_NRC_UploadDownloadNotAccepted:
    case UDS_NRC_TransferDataSuspended:
    case UDS_NRC_GeneralProgrammingFailure:
    case UDS_NRC_WrongBlockSequenceCounter:
    case UDS_NRC_RequestCorrectlyReceived_ResponsePending:
    case UDS_NRC_SubFunctionNotSupportedInActiveSession:
    case UDS_NRC_ServiceNotSupportedInActiveSession:
    case UDS_NRC_RpmTooHigh:
    case UDS_NRC_RpmTooLow:
    case UDS_NRC_EngineIsRunning:
    case UDS_NRC_EngineIsNotRunning:
    case UDS_NRC_EngineRunTimeTooLow:
    case UDS_NRC_TemperatureTooHigh:
    case UDS_NRC_TemperatureTooLow:
    case UDS_NRC_VehicleSpeedTooHigh:
    case UDS_NRC_VehicleSpeedTooLow:
    case UDS_NRC_ThrottlePedalTooHigh:
    case UDS_NRC_ThrottlePedalTooLow:
    case UDS_NRC_TransmissionRangeNotInNeutral:
    case UDS_NRC_TransmissionRangeNotInGear:
    case UDS_NRC_BrakeSwitchNotClosed:
    case UDS_NRC_ShifterLeverNotInPark:
    case UDS_NRC_TorqueConverterClutchLocked:
    case UDS_NRC_VoltageTooHigh:
    case UDS_NRC_VoltageTooLow:
    case UDS_NRC_ResourceTemporarilyNotAvailable:
        return true;
    default:
        return false;
    }
}


#ifdef UDS_LINES
#line 1 "src/log.c"
#endif
#include <stdio.h>
#include <stdarg.h>

#if UDS_LOG_LEVEL > UDS_LOG_NONE
void UDS_LogWrite(UDS_LogLevel_t level, const char *tag, const char *format, ...) {
    va_list list;
    (void)level;
    (void)tag;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}

void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer,
                        size_t buff_len, UDSSDU_t *info) {
    (void)info;
    for (unsigned i = 0; i < buff_len; i++) {
        UDS_LogWrite(level, tag, "%02x ", buffer[i]);
    }
    UDS_LogWrite(level, tag, "\n");
}
#endif


#ifdef UDS_LINES
#line 1 "src/tp/isotp_c.c"
#endif
#if defined(UDS_TP_ISOTP_C)


static UDSTpStatus_t tp_poll(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    UDSTpStatus_t status = 0;
    UDSISOTpC_t *impl = (UDSISOTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

static ssize_t tp_send(UDSTp_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ssize_t ret = -1;
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;
    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &tp->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &tp->func_link;
        if (len > 7) {
            UDS_LOGI(__FILE__, "Cannot send more than 7 bytes via functional addressing\n");
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

static ssize_t tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(buf);
    uint16_t out_size = 0;
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;

    int ret = isotp_receive(&tp->phys_link, buf, bufsize, &out_size);
    if (ret == ISOTP_RET_OK) {
        UDS_LOGI(__FILE__, "phys link received %d bytes", out_size);
        if (NULL != info) {
            info->A_TA = tp->phys_sa;
            info->A_SA = tp->phys_ta;
            info->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
        }
    } else if (ret == ISOTP_RET_NO_DATA) {
        ret = isotp_receive(&tp->func_link, buf, bufsize, &out_size);
        if (ret == ISOTP_RET_OK) {
            UDS_LOGI(__FILE__, "func link received %d bytes", out_size);
            if (NULL != info) {
                info->A_TA = tp->func_sa;
                info->A_SA = tp->func_ta;
                info->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
            }
        } else if (ret == ISOTP_RET_NO_DATA) {
            return 0;
        } else {
            UDS_LOGE(__FILE__, "unhandled return code from func link %d\n", ret);
        }
    } else {
        UDS_LOGE(__FILE__, "unhandled return code from phys link %d\n", ret);
    }
    return out_size;
}

UDSErr_t UDSISOTpCInit(UDSISOTpC_t *tp, const UDSISOTpCConfig_t *cfg) {
    if (cfg == NULL || tp == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    tp->hdl.poll = tp_poll;
    tp->hdl.send = tp_send;
    tp->hdl.recv = tp_recv;
    tp->phys_sa = cfg->source_addr;
    tp->phys_ta = cfg->target_addr;
    tp->func_sa = cfg->source_addr_func;
    tp->func_ta = cfg->target_addr_func;

    isotp_init_link(&tp->phys_link, tp->phys_ta, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
    isotp_init_link(&tp->func_link, tp->func_ta, tp->recv_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
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
    struct sockaddr_can addr = {0};
    struct ifreq ifr = {0};
    int sockfd = -1;

    if ((sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("socket");
        goto done;
    }

    memset(&ifr, 0, sizeof(ifr));
    if (snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname) >= (int)sizeof(ifr.ifr_name)) {
        UDS_LOGE(__FILE__, "Interface name too long");
        close(sockfd);
        sockfd = -1;
        goto done;
    }
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

uint32_t isotp_user_get_us(void) { return UDSMillis() * 1000; }

__attribute__((format(printf, 1, 2))) void isotp_user_debug(const char *message, ...) {
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
}

#ifndef ISO_TP_USER_SEND_CAN_ARG
#error "ISO_TP_USER_SEND_CAN_ARG must be defined"
#endif
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    (void)fflush(stdout);
    UDS_ASSERT(user_data);
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
    UDS_ASSERT(tp);
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
                isotp_on_can_message(&tp->phys_link, frame.data, frame.can_dlc);
            } else if (frame.can_id == tp->func_sa) {
                if (ISOTP_RECEIVE_STATUS_IDLE != tp->phys_link.receive_status) {
                    UDS_LOGI(__FILE__,
                             "func frame received but cannot process because link is not idle");
                    return;
                }
                // TODO: reject if it's longer than a single frame
                isotp_on_can_message(&tp->func_link, frame.data, frame.can_dlc);
            }
        }
    }
}

static UDSTpStatus_t isotp_c_socketcan_tp_poll(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    UDSTpStatus_t status = 0;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    SocketCANRecv(impl);
    isotp_poll(&impl->phys_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_ERROR) {
        status |= UDS_TP_ERR;
    }
    return status;
}

static ssize_t isotp_c_socketcan_tp_send(UDSTp_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
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
            UDS_LOGI(__FILE__, "Cannot send more than 7 bytes via functional addressing");
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
    UDS_LOGD(__FILE__, "'%s' sends %ld bytes to 0x%03x (%s)", tp->tag, len, ta,
             ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    UDS_LOG_SDU(__FILE__, buf, len, info);
    return ret;
}

static ssize_t isotp_c_socketcan_tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize,
                                         UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(buf);
    uint16_t out_size = 0;
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;

    int ret = isotp_receive(&tp->phys_link, buf, bufsize, &out_size);
    if (ret == ISOTP_RET_OK) {
        UDS_LOGI(__FILE__, "phys link received %d bytes", out_size);
        if (NULL != info) {
            info->A_TA = tp->phys_sa;
            info->A_SA = tp->phys_ta;
            info->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
        }
    } else if (ret == ISOTP_RET_NO_DATA) {
        ret = isotp_receive(&tp->func_link, buf, bufsize, &out_size);
        if (ret == ISOTP_RET_OK) {
            UDS_LOGI(__FILE__, "func link received %d bytes", out_size);
            if (NULL != info) {
                info->A_TA = tp->func_sa;
                info->A_SA = tp->func_ta;
                info->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
            }
        } else if (ret == ISOTP_RET_NO_DATA) {
            return 0;
        } else {
            UDS_LOGE(__FILE__, "unhandled return code from func link %d\n", ret);
        }
    } else {
        UDS_LOGE(__FILE__, "unhandled return code from phys link %d\n", ret);
    }
    return out_size;
}

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, const char *ifname, uint32_t source_addr,
                         uint32_t target_addr, uint32_t source_addr_func,
                         uint32_t target_addr_func) {
    UDS_ASSERT(tp);
    UDS_ASSERT(ifname);
    tp->hdl.poll = isotp_c_socketcan_tp_poll;
    tp->hdl.send = isotp_c_socketcan_tp_send;
    tp->hdl.recv = isotp_c_socketcan_tp_recv;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;
    tp->func_ta = target_addr;
    tp->fd = SetupSocketCAN(ifname);

    isotp_init_link(&tp->phys_link, target_addr, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
    isotp_init_link(&tp->func_link, target_addr_func, tp->recv_buf, sizeof(tp->send_buf),
                    tp->recv_buf, sizeof(tp->recv_buf));

    tp->phys_link.user_send_can_arg = &(tp->fd);
    tp->func_link.user_send_can_arg = &(tp->fd);

    return UDS_OK;
}

void UDSTpISOTpCDeinit(UDSTpISOTpC_t *tp) {
    UDS_ASSERT(tp);
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
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static UDSTpStatus_t isotp_sock_tp_poll(UDSTp_t *hdl) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    UDSTpStatus_t status = 0;
    int ret = 0;
    int fds[2] = {impl->phys_fd, impl->func_fd};
    struct pollfd pfds[2] = {0};
    pfds[0].fd = impl->phys_fd;
    pfds[0].events = POLLERR | POLLOUT;
    pfds[0].revents = 0;

    pfds[1].fd = impl->func_fd;
    pfds[1].events = POLLERR | POLLOUT;
    pfds[1].revents = 0;

    ret = poll(pfds, 2, 1);
    if (ret < 0) {
        UDS_LOGE(__FILE__, "poll failed: %d", ret);
        status |= UDS_TP_ERR;
    } else if (ret == 0) {
        ; // timeout, no events
    } else {
        // poll() returned with events
        for (int i = 0; i < 2; i++) {
            struct pollfd pfd = pfds[i];

            // Check for errors
            if (pfd.revents & POLLERR) {
                int pending_err = 0;
                socklen_t len = sizeof(pending_err);
                if (!getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &pending_err, &len) && pending_err) {
                    switch (pending_err) {
                    case ECOMM:
                        UDS_LOGE(__FILE__, "ECOMM: Communication error on send");
                        status |= UDS_TP_ERR;
                        break;
                    default:
                        UDS_LOGE(__FILE__, "Asynchronous socket error: %s (%d)",
                                 strerror(pending_err), pending_err);
                        status |= UDS_TP_ERR;
                        break;
                    }
                } else {
                    UDS_LOGE(__FILE__, "POLLERR was set, but no error returned via SO_ERROR?");
                }
            }

            // Check if send is in progress on physical socket
            // Only check the physical socket (not functional) since that's what sends multi-frame
            if (fds[i] == impl->phys_fd && pfd.revents != 0) {
                // When POLLOUT is NOT set but other events are present, the socket cannot accept
                // writes because a multi-frame transmission is in progress.
                // See: https://lore.kernel.org/all/20230331125511.372783-1-michal.sojka@cvut.cz/
                // The kernel ISO-TP driver suppresses POLLOUT when tx.state != ISOTP_IDLE
                if (!(pfd.revents & POLLOUT)) {
                    status |= UDS_TP_SEND_IN_PROGRESS;
                }
            }
        }
    }
    return status;
}

static ssize_t tp_recv_once(int fd, uint8_t *buf, size_t size) {
    ssize_t ret = read(fd, buf, size);
    if (ret < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            ret = 0;
        } else {
            UDS_LOGI(__FILE__, "read failed: %ld with errno: %d\n", ret, errno);
            if (EILSEQ == errno) {
                UDS_LOGI(__FILE__, "Perhaps I received multiple responses?");
            }
        }
    }
    return ret;
}

static ssize_t isotp_sock_tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(buf);
    ssize_t ret = 0;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    UDSSDU_t *msg = &impl->recv_info;

    ret = tp_recv_once(impl->phys_fd, buf, bufsize);
    if (ret > 0) {
        msg->A_TA = impl->phys_sa;
        msg->A_SA = impl->phys_ta;
        msg->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
    } else {
        ret = tp_recv_once(impl->func_fd, buf, bufsize);
        if (ret > 0) {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
            msg->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
        }
    }

    if (ret > 0) {
        if (info) {
            *info = *msg;
        }

        UDS_LOGD(__FILE__, "'%s' received %ld bytes from 0x%03x (%s), ", impl->tag, ret, msg->A_TA,
                 msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        UDS_LOG_SDU(__FILE__, impl->recv_buf, ret, msg);
    }

    return ret;
}

static ssize_t isotp_sock_tp_send(UDSTp_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ssize_t ret = -1;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    int fd;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        fd = impl->phys_fd;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {
        if (len > 7) {
            UDS_LOGI(__FILE__, "UDSTpIsoTpSock: functional request too large");
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
done:;
    int ta = ta_type == UDS_A_TA_TYPE_PHYSICAL ? impl->phys_ta : impl->func_ta;
    UDS_LOGD(__FILE__, "'%s' sends %ld bytes to 0x%03x (%s)", impl->tag, len, ta,
             ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    UDS_LOG_SDU(__FILE__, buf, len, info);

    return ret;
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

    struct can_isotp_options opts;
    memset(&opts, 0, sizeof(opts));

    if (functional) {
        UDS_LOGI(__FILE__, "configuring fd: %d as functional", fd);
        // configure the socket as listen-only to avoid sending FC frames
        opts.flags |= CAN_ISOTP_LISTEN_MODE;
    }

    if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0) {
        perror("setsockopt (isotp_options):");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    if (snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", if_name) >= (int)sizeof(ifr.ifr_name)) {
        UDS_LOGE(__FILE__, "Interface name too long");
        close(fd);
        return -1;
    }
    ioctl(fd, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_addr.tp.rx_id = rxid;
    addr.can_addr.tp.tx_id = txid;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        UDS_LOGI(__FILE__, "Bind: %s %s\n", strerror(errno), if_name);
        return -1;
    }
    return fd;
}

UDSErr_t UDSTpIsoTpSockInitServer(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func) {
    UDS_ASSERT(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.recv = isotp_sock_tp_recv;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, source_addr_func, 0, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        UDS_LOGI(__FILE__, "foo\n");
        (void)fflush(stdout);
        return UDS_FAIL;
    }
    const char *tag = "server";
    memmove(tp->tag, tag, strlen(tag));
    UDS_LOGI(__FILE__, "%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x",
             strlen(tp->tag) ? tp->tag : "server", source_addr, target_addr, source_addr_func,
             target_addr);
    return UDS_OK;
}

UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func) {
    UDS_ASSERT(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.recv = isotp_sock_tp_recv;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->func_ta = target_addr_func;
    tp->phys_ta = target_addr;
    tp->phys_sa = source_addr;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, 0, target_addr_func, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        return UDS_FAIL;
    }
    const char *tag = "client";
    memmove(tp->tag, tag, strlen(tag));
    UDS_LOGI(__FILE__,
             "%s initialized phys link (fd %d) rx 0x%03x tx 0x%03x func link (fd %d) rx 0x%03x tx "
             "0x%03x",
             strlen(tp->tag) ? tp->tag : "client", tp->phys_fd, source_addr, target_addr,
             tp->func_fd, source_addr, target_addr_func);
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
#line 1 "src/tp/isotp_mock.c"
#endif
#if defined(UDS_TP_ISOTP_MOCK)

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NUM_TP 16
#define NUM_MSGS 8
static ISOTPMock_t *TPs[MAX_NUM_TP];
static unsigned TPCount = 0;
static FILE *LogFile = NULL;
static struct Msg {
    uint8_t buf[UDS_ISOTP_MTU];
    size_t len;
    UDSSDU_t info;
    uint32_t scheduled_tx_time;
    ISOTPMock_t *sender;
} msgs[NUM_MSGS];
static unsigned MsgCount = 0;

static void NetworkPoll(void) {
    for (unsigned i = 0; i < MsgCount; i++) {
        if (UDSTimeAfter(UDSMillis(), msgs[i].scheduled_tx_time)) {
            bool found = false;
            for (unsigned j = 0; j < TPCount; j++) {
                ISOTPMock_t *tp = TPs[j];
                if (tp->sa_phys == msgs[i].info.A_TA || tp->sa_func == msgs[i].info.A_TA) {
                    found = true;
                    if (tp->recv_len > 0) {
                        UDS_LOGW(__FILE__,
                                 "TPMock: %s recv buffer is already full. Message dropped",
                                 tp->name);
                        continue;
                    }

                    UDS_LOGD(__FILE__,
                             "%s receives %ld bytes from TA=0x%03X (A_TA_Type=%s):", tp->name,
                             msgs[i].len, msgs[i].info.A_TA,
                             msgs[i].info.A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "PHYSICAL"
                                                                              : "FUNCTIONAL");
                    UDS_LOG_SDU(__FILE__, msgs[i].buf, msgs[i].len, &(msgs[i].info));

                    memmove(tp->recv_buf, msgs[i].buf, msgs[i].len);
                    tp->recv_len = msgs[i].len;
                    tp->recv_info = msgs[i].info;
                }
            }

            if (!found) {
                UDS_LOGW(__FILE__, "TPMock: no matching receiver for message");
            }

            for (unsigned j = i + 1; j < MsgCount; j++) {
                msgs[j - 1] = msgs[j];
            }
            MsgCount--;
            i--;
        }
    }
}

static ssize_t mock_tp_send(struct UDSTp *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl;
    if (MsgCount >= NUM_MSGS) {
        UDS_LOGW(__FILE__, "mock_tp_send: too many messages in the queue");
        return -1;
    }
    struct Msg *m = &msgs[MsgCount++];
    UDSTpAddr_t ta_type =
        info == NULL ? (UDSTpAddr_t)UDS_A_TA_TYPE_PHYSICAL : (UDSTpAddr_t)info->A_TA_Type;
    m->len = len;
    m->info.A_AE = info == NULL ? 0 : info->A_AE;
    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        m->info.A_TA = tp->ta_phys;
        m->info.A_SA = tp->sa_phys;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {

        // This condition is only true for standard CAN.
        // Technically CAN-FD may also be used in ISO-TP.
        // TODO: add profiles to isotp_mock
        if (len > 7) {
            UDS_LOGW(__FILE__, "mock_tp_send: functional message too long: %ld", len);
            return -1;
        }
        m->info.A_TA = tp->ta_func;
        m->info.A_SA = tp->sa_func;
    } else {
        UDS_LOGW(__FILE__, "mock_tp_send: unknown TA type: %d", ta_type);
        return -1;
    }
    m->info.A_TA_Type = ta_type;
    m->scheduled_tx_time = UDSMillis() + tp->send_tx_delay_ms;
    memmove(m->buf, buf, len);

    UDS_LOGD(__FILE__, "%s sends %ld bytes to TA=0x%03X (A_TA_Type=%s):", tp->name, len,
             m->info.A_TA, m->info.A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "PHYSICAL" : "FUNCTIONAL");
    UDS_LOG_SDU(__FILE__, buf, len, &m->info);

    return len;
}

static ssize_t mock_tp_recv(struct UDSTp *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl;
    if (tp->recv_len == 0) {
        return 0;
    }
    if (bufsize < tp->recv_len) {
        UDS_LOGW(__FILE__, "mock_tp_recv: buffer too small: %ld < %ld", bufsize, tp->recv_len);
        return -1;
    }
    ssize_t len = (ssize_t)tp->recv_len;
    memmove(buf, tp->recv_buf, tp->recv_len);
    if (info) {
        *info = tp->recv_info;
    }
    tp->recv_len = 0;
    return len;
}

static UDSTpStatus_t mock_tp_poll(struct UDSTp *hdl) {
    (void)hdl; // unused parameter
    NetworkPoll();
    // todo: make this status reflect TX time
    return UDS_TP_IDLE;
}

static_assert(offsetof(ISOTPMock_t, hdl) == 0, "ISOTPMock_t must not have any members before hdl");

static void ISOTPMockAttach(ISOTPMock_t *tp, ISOTPMockArgs_t *args) {
    UDS_ASSERT(tp);
    UDS_ASSERT(args);
    UDS_ASSERT(TPCount < MAX_NUM_TP);
    TPs[TPCount++] = tp;
    tp->hdl.send = mock_tp_send;
    tp->hdl.recv = mock_tp_recv;
    tp->hdl.poll = mock_tp_poll;
    tp->sa_func = args->sa_func;
    tp->sa_phys = args->sa_phys;
    tp->ta_func = args->ta_func;
    tp->ta_phys = args->ta_phys;
    tp->recv_len = 0;
    UDS_LOGV(__FILE__, "attached %s. TPCount: %d", tp->name, TPCount);
}

static void ISOTPMockDetach(ISOTPMock_t *tp) {
    UDS_ASSERT(tp);
    for (unsigned i = 0; i < TPCount; i++) {
        if (TPs[i] == tp) {
            for (unsigned j = i + 1; j < TPCount; j++) {
                TPs[j - 1] = TPs[j];
            }
            TPCount--;
            UDS_LOGV(__FILE__, "TPMock: detached %s. TPCount: %d", tp->name, TPCount);
            return;
        }
    }
    UDS_ASSERT(false);
}

UDSTp_t *ISOTPMockNew(const char *name, ISOTPMockArgs_t *args) {
    if (TPCount >= MAX_NUM_TP) {
        UDS_LOGI(__FILE__, "TPCount: %d, too many TPs\n", TPCount);
        return NULL;
    }
    ISOTPMock_t *tp = malloc(sizeof(ISOTPMock_t));
    memset(tp, 0, sizeof(ISOTPMock_t));
    if (name) {
        if (snprintf(tp->name, sizeof(tp->name), "%s", name) >= (int)sizeof(tp->name)) {
            UDS_LOGE(__FILE__, "Transport name too long, truncated");
        }
    } else {
        (void)snprintf(tp->name, sizeof(tp->name), "TPMock%u", TPCount);
    }
    ISOTPMockAttach(tp, args);
    return &tp->hdl;
}

void ISOTPMockConnect(UDSTp_t *tp1, UDSTp_t *tp2);

void ISOTPMockLogToFile(const char *filename) {
    if (LogFile) {
        (void)fprintf(stderr, "Log file is already open\n");
        return;
    }
    if (!filename) {
        (void)fprintf(stderr, "Filename is NULL\n");
        return;
    }
    // create file
    LogFile = fopen(filename, "w");
    if (!LogFile) {
        (void)fprintf(stderr, "Failed to open log file %s\n", filename);
        return;
    }
}

void ISOTPMockLogToStdout(void) {
    if (LogFile) {
        return;
    }
    LogFile = stdout;
}

void ISOTPMockReset(void) {
    memset(TPs, 0, sizeof(TPs));
    TPCount = 0;
    memset(msgs, 0, sizeof(msgs));
    MsgCount = 0;
}

void ISOTPMockFree(UDSTp_t *tp) {
    ISOTPMock_t *tpm = (ISOTPMock_t *)tp;
    ISOTPMockDetach(tpm);
    free(tp);
}

#endif

#if defined(UDS_TP_ISOTP_C)
#ifndef ISO_TP_USER_SEND_CAN_ARG
#error
#endif
#include <stdint.h>

///////////////////////////////////////////////////////
///                 STATIC FUNCTIONS                ///
///////////////////////////////////////////////////////

/* st_min to microsecond */
static uint8_t isotp_us_to_st_min(uint32_t us) {
    if (us <= 127000) {
        if (us >= 100 && us <= 900) {
            return (uint8_t)(0xF0 + (us / 100));
        } else {
            return (uint8_t)(us / 1000u);
        }
    }

    return 0;
}

/* st_min to usec  */
static uint32_t isotp_st_min_to_us(uint8_t st_min) {
    if (st_min <= 0x7F) {
        return st_min * 1000;
    } else if (st_min >= 0xF1 && st_min <= 0xF9) {
        return (st_min - 0xF0) * 100;
    }
    return 0;
}

static int isotp_send_flow_control(const IsoTpLink* link, uint8_t flow_status, uint8_t block_size, uint32_t st_min_us) {

    IsoTpCanMessage message;
    int ret;
    uint8_t size = 0;

    /* setup message  */
    message.as.flow_control.type = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.FS = flow_status;
    message.as.flow_control.BS = block_size;
    message.as.flow_control.STmin = isotp_us_to_st_min(st_min_us);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void) memset(message.as.flow_control.reserve, ISO_TP_FRAME_PADDING_VALUE, sizeof(message.as.flow_control.reserve));
    size = sizeof(message);
#else
    size = 3;
#endif

    ret = isotp_user_send_can(link->send_arbitration_id, message.as.data_array.ptr, size
    #if defined (ISO_TP_USER_SEND_CAN_ARG)
    ,link->user_send_can_arg
    #endif
    );

    return ret;
}

static int isotp_send_single_frame(const IsoTpLink* link, uint32_t id) {

    IsoTpCanMessage message;
    int ret;
    uint8_t size = 0;
    (void)id;

    /* multi frame message length must greater than 7  */
    assert(link->send_size <= 7);

    /* setup message  */
    message.as.single_frame.type = ISOTP_PCI_TYPE_SINGLE;
    message.as.single_frame.SF_DL = (uint8_t) link->send_size;
    (void) memcpy(message.as.single_frame.data, link->send_buffer, link->send_size);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void) memset(message.as.single_frame.data + link->send_size, ISO_TP_FRAME_PADDING_VALUE, sizeof(message.as.single_frame.data) - link->send_size);
    size = sizeof(message);
#else
    size = link->send_size + 1;
#endif

    ret = isotp_user_send_can(link->send_arbitration_id, message.as.data_array.ptr, size
    #if defined (ISO_TP_USER_SEND_CAN_ARG)
    ,link->user_send_can_arg
    #endif
    );

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
    ret = isotp_user_send_can(id, message.as.data_array.ptr, sizeof(message) 
    #if defined (ISO_TP_USER_SEND_CAN_ARG)
    ,link->user_send_can_arg
    #endif

    );
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
    uint8_t size = 0;

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
    (void) memset(message.as.consecutive_frame.data + data_length, ISO_TP_FRAME_PADDING_VALUE, sizeof(message.as.consecutive_frame.data) - data_length);
    size = sizeof(message);
#else
    size = data_length + 1;
#endif

    ret = isotp_user_send_can(link->send_arbitration_id,
            message.as.data_array.ptr, size
#if defined (ISO_TP_USER_SEND_CAN_ARG)
    ,link->user_send_can_arg
#endif
    );

    if (ISOTP_RET_OK == ret) {
        link->send_offset += data_length;
        if (++(link->send_sn) > 0x0F) {
            link->send_sn = 0;
        }
    }
    
    return ret;
}

static int isotp_receive_single_frame(IsoTpLink* link, const IsoTpCanMessage* message, uint8_t len) {
    /* check data length */
    if ((0 == message->as.single_frame.SF_DL) || (message->as.single_frame.SF_DL > (len - 1))) {
        isotp_user_debug("Single-frame length too small.");
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
        isotp_user_debug("First frame should be 8 bytes in length.");
        return ISOTP_RET_LENGTH;
    }

    /* check data length */
    payload_length = message->as.first_frame.FF_DL_high;
    payload_length = (uint16_t)(payload_length << 8) + message->as.first_frame.FF_DL_low;

    /* should not use multiple frame transmition */
    if (payload_length <= 7) {
        isotp_user_debug("Should not use multiple frame transmission.");
        return ISOTP_RET_LENGTH;
    }
    
    if (payload_length > link->receive_buf_size) {
        isotp_user_debug("Multi-frame response too large for receiving buffer.");
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
        isotp_user_debug("Consecutive frame too short.");
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
    /* unused args */
    (void) link;
    (void) message;

    /* check message length */
    if (len < 3) {
        isotp_user_debug("Flow control frame too short.");
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
        isotp_user_debug("Link is null!");
        return ISOTP_RET_ERROR;
    }

    if (size > link->send_buf_size) {
        isotp_user_debug("Message size too large. Increase ISO_TP_MAX_MESSAGE_SIZE to set a larger buffer\n");
        const int32_t messageSize = 128;
        char message[messageSize];
        int32_t writtenChars = sprintf(&message[0], "Attempted to send %d bytes; max size is %d!\n", size, link->send_buf_size);

        assert(writtenChars <= messageSize);
        (void) writtenChars;
        
        isotp_user_debug("%s", message);
        return ISOTP_RET_OVERFLOW;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status) {
        isotp_user_debug("Abort previous message, transmission in progress.\n");
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
            link->send_st_min_us = 0;
            link->send_wtf_count = 0;
            link->send_timer_st = isotp_user_get_us();
            link->send_timer_bs = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
            link->send_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            link->send_status = ISOTP_SEND_STATUS_INPROGRESS;
        }
    }

    return ret;
}

void isotp_on_can_message(IsoTpLink* link, const uint8_t* data, uint8_t len) {
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
                isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN_US);
                /* refresh timer cs */
                link->receive_timer_cr = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
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
                link->receive_timer_cr = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
                
                /* receive finished */
                if (link->receive_offset >= link->receive_size) {
                    link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
                } else {
                    /* send fc when bs reaches limit */
                    if (0 == --link->receive_bs_count) {
                        link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                        isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN_US);
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
                link->send_timer_bs = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;

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
                    uint32_t message_st_min_us = isotp_st_min_to_us(message.as.flow_control.STmin);
                    link->send_st_min_us = message_st_min_us > ISO_TP_DEFAULT_ST_MIN_US ? message_st_min_us : ISO_TP_DEFAULT_ST_MIN_US; // prefer as much st_min as possible for stability?
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

void isotp_init_link(IsoTpLink *link, uint32_t sendid, uint8_t *sendbuf, uint16_t sendbufsize, uint8_t *recvbuf, uint16_t recvbufsize) {
    memset(link, 0, sizeof(*link));
    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    link->send_status = ISOTP_SEND_STATUS_IDLE;
    link->send_arbitration_id = sendid;
    link->send_buffer = sendbuf;
    link->send_buf_size = sendbufsize;
    link->receive_buffer = recvbuf;
    link->receive_buf_size = recvbufsize;
    
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
        (0 == link->send_st_min_us || IsoTpTimeAfter(isotp_user_get_us(), link->send_timer_st))) {
            
            ret = isotp_send_consecutive_frame(link);
            if (ISOTP_RET_OK == ret) {
                if (ISOTP_INVALID_BS != link->send_bs_remain) {
                    link->send_bs_remain -= 1;
                }
                link->send_timer_bs = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
                link->send_timer_st = isotp_user_get_us() + link->send_st_min_us;

                /* check if send finish */
                if (link->send_offset >= link->send_size) {
                    link->send_status = ISOTP_SEND_STATUS_IDLE;
                }
            } else if (ISOTP_RET_NOSPACE == ret) {
                /* shim reported that it isn't able to send a frame at present, retry on next call */
            } else {
                link->send_status = ISOTP_SEND_STATUS_ERROR;
            }
        }

        /* check timeout */
        if (IsoTpTimeAfter(isotp_user_get_us(), link->send_timer_bs)) {
            link->send_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_BS;
            link->send_status = ISOTP_SEND_STATUS_ERROR;
        }
    }

    /* only polling when operation in progress */
    if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {
        
        /* check timeout */
        if (IsoTpTimeAfter(isotp_user_get_us(), link->receive_timer_cr)) {
            link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_CR;
            link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
        }
    }

    return;
}
#endif

