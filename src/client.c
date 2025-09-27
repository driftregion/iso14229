#include "client.h"
#include "config.h"
#include "uds.h"
#include "util.h"
#include "log.h"

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

    if (NULL == data) {
        return UDS_ERR_INVALID_ARG;
    }
    if (size > sizeof(client->send_buf) - UDS_0X27_REQ_BASE_LEN) {
        return UDS_ERR_BUFSIZ;
    }
    if (size == 0 && NULL != data) {
        UDS_LOGE(__FILE__, "size == 0 and data is non-null");
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
