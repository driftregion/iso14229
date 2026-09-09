/**
 * @file iso14229.c
 * @brief ISO14229-1 (UDS) library
 * @copyright Copyright (c) Nick Kirkby
 * @see https://github.com/driftregion/iso14229
 */

#include "iso14229.h"

#ifdef UDS_LINES
#line 1 "src/util_private.h"
#endif





/// Serializes n bytes of val to *dst in big-endian format.
static inline void PackBE(uint8_t *dst, uint64_t val, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = (uint8_t)(val >> (8 * (n - 1 - i)));
    }
}

/**
 * @brief Unpack up to sizeof(size_t) big-endian bytes from src into dst.
 * @param src buffer
 * @param dst
 * @param n ranges from 0 to sizeof(size_t) inclusive
 * @return UDS_OK if successful
 */
static inline UDSErr_t UnpackBEsize(const uint8_t *src, size_t *dst, size_t n) {
    if (NULL == src || NULL == dst || n > sizeof(*dst)) {
        return UDS_ERR_INVALID_ARG;
    }
    size_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    *dst = val;
    return UDS_OK;
}

/**
 * @brief Unpack up to sizeof(uintptr_t) big-endian bytes from src into dst.
 * @param src buffer
 * @param dst
 * @param n ranges from 0 to sizeof(uintptr_t) inclusive
 * @return UDS_OK if successful
 */
static inline UDSErr_t UnpackBEuintptr(const uint8_t *src, uintptr_t *dst, size_t n) {
    if (NULL == src || NULL == dst || n > sizeof(*dst)) {
        return UDS_ERR_INVALID_ARG;
    }
    uintptr_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    *dst = val;
    return UDS_OK;
}

/**
 * @brief Unpack up to 4 big-endian bytes from src into dst as uint32_t.
 * @param src buffer
 * @param dst pointer to destination
 * @param n ranges from 0 to 4 inclusive
 * @return UDS_OK if successful
 */
static inline UDSErr_t UnpackBEu32(const uint8_t *src, uint32_t *dst, size_t n) {
    if (NULL == src || NULL == dst || n > sizeof(*dst)) {
        return UDS_ERR_INVALID_ARG;
    }
    uint32_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    *dst = val;
    return UDS_OK;
}

/// returns true if a security level is reserved per ISO14229-1:2020 Table 42
bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel);

/// returns true if err is defined in ISO14229-1:2020 as an NRC
bool UDSErrIsNRC(UDSErr_t err);


#ifdef UDS_LINES
#line 1 "src/client.c"
#endif







#include <stdint.h>

/**
 * @defgroup client_request_states valid values of UDSClient_t::state
 * @brief internal state machine states for a single client request
 * @see UDSClient_t::state
 * @{
 */
#define STATE_IDLE 0                /**< no request in progress */
#define STATE_SENDING 1             /**< request is being transmitted */
#define STATE_AWAIT_SEND_COMPLETE 2 /**< waiting for the transport to finish sending */
#define STATE_AWAIT_RESPONSE 3      /**< request sent, awaiting server response */
/** @} */

UDSErr_t UDSClientInit(UDSClient_t *client) {
    if (NULL == client) {
        return UDS_ERR_INVALID_ARG;
    }
    memset(client, 0, sizeof(*client));
    client->state = STATE_IDLE;
    client->cfg_data_format_identifier = 0x00;
    client->cfg_file_size_parameter_length = 0x04;

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
        UDS_LOGV(__FILE__, "client state: %s (%d) -> %s (%d)", ClientStateName(client->state),
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
            uint32_t p2_star = (uint32_t)((client->recv_buf[4] << 8) + client->recv_buf[5]) * 10U;
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

    UDSTpPoll(client->tp);
    switch (client->state) {
    case STATE_IDLE: {
        client->options = client->defaultOptions;
        break;
    }
    case STATE_SENDING: {
        {
            // warn if anything is received. TODO (easy): make into a function
            // While sending, we expect that nothing should be received,
            // but sometimes data is received due to e.g. misconfiguration.
            UDSSDU_t info = {0};
            size_t recvlen = 0;
            UDSErr_t err =
                UDSTpRecv(client->tp, client->recv_buf, sizeof(client->recv_buf), &recvlen, &info);
            if (UDS_OK != err) {
                UDS_LOGE(__FILE__, "transport returned error %s", UDSErrToStr(err));
            } else if (recvlen == 0) {
                ; // expected
            } else {
                UDS_LOGW(__FILE__, "received %zd unexpected bytes:", recvlen);
                UDS_LOG_SDU(__FILE__, client->recv_buf, recvlen, &info);
            }
        }

        memset(client->recv_buf, 0, sizeof(client->recv_buf));
        client->recv_size = 0;

        UDS_A_TA_Type_t ta_type = client->_options_copy & UDS_FUNCTIONAL ? UDS_A_TA_TYPE_FUNCTIONAL
                                                                         : UDS_A_TA_TYPE_PHYSICAL;
        UDSSDU_t info = {
            .A_Mtype = UDS_A_MTYPE_DIAG,
            .A_TA_Type = ta_type,
        };
        err = UDSTpSend(client->tp, client->send_buf, client->send_size, &info);
        if (UDS_OK != err) {
            UDS_LOGE(__FILE__, "tport err: %s", UDSErrToStr(err));
        } else {
            changeState(client, STATE_AWAIT_SEND_COMPLETE);
        }
        break;
    }
    case STATE_AWAIT_SEND_COMPLETE: {
        if (client->_options_copy & UDS_FUNCTIONAL) {
            // "The Functional addressing is applied only to single frame transmission"
            // Specification of Diagnostic Communication (Diagnostic on CAN - Network Layer)
            changeState(client, STATE_IDLE);
        }
        if (client->tp->status.is_sending) {
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
        size_t recvlen = 0;
        err = UDSTpRecv(client->tp, client->recv_buf, sizeof(client->recv_buf), &recvlen, &info);
        if (UDS_OK != err) {
            changeState(client, STATE_IDLE);
        } else if (0 == recvlen) {
            if (UDSTimeAfter(UDSMillis(), client->p2_timer)) {
                UDS_LOGI(__FILE__, "p2 timeout");
                err = UDS_ERR_TIMEOUT;
                changeState(client, STATE_IDLE);
            }
        } else {
            UDS_LOGD(__FILE__, "received %zd bytes. Processing...", recvlen);
            UDS_ASSERT(len <= (UDSTpSsize_t)UINT16_MAX);
            client->recv_size = recvlen;

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
    uint16_t n_filePathLen = (uint16_t)filePathLenSize;

    /*
    Pre-compute the request length based on the MOOP.
    For each field, "Y" denotes present and "_" denotes absent.

                        MOOP    1   2   3   4   5   6
    field                                                   size (bytes)
    Request SID                 Y   Y   Y   Y   Y   Y       1
    modeOfOperation             Y   Y   Y   Y   Y   Y       1
    filePathAndNameLength       Y   Y   Y   Y   Y   Y       2
    filePathAndName             Y   Y   Y   Y   Y   Y       n_filePathLen
    dataFormatIdentifier        Y   _   Y   Y   _   Y       1
    fileSizeParameterLength     Y   _   Y   _   _   Y       1
    fileSizeUncompressed        Y   _   Y   _   _   Y       cfg_file_size_parameter_length
    fileSizeCompressed          Y   _   Y   _   _   Y       cfg_file_size_parameter_length
    */

    if (sizeof(client->send_buf) < UDS_0X38_REQ_BASE_LEN) {
        return UDS_ERR_BUFSIZ;
    }

    size_t bufSizeRequired = SIZE_MAX;
    client->send_buf[0] = kSID_REQUEST_FILE_TRANSFER; // Request SID
    client->send_buf[1] = mode;                       // modeOfOperation
    PackBE(&client->send_buf[2], n_filePathLen, 2);   // filePathAndNameLength

    switch (mode) {
    case UDS_MOOP_ADDFILE: // 1
        bufSizeRequired = 4 + n_filePathLen + 2 + 2 * client->cfg_file_size_parameter_length;
        if (bufSizeRequired > sizeof(client->send_buf)) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[4], filePath, n_filePathLen); // filePathAndName
        client->send_buf[4 + n_filePathLen] = client->cfg_data_format_identifier;
        client->send_buf[5 + n_filePathLen] = client->cfg_file_size_parameter_length;
        PackBE(&client->send_buf[6 + n_filePathLen], fileSizeUncompressed,
               client->cfg_file_size_parameter_length);
        PackBE(&client->send_buf[6 + n_filePathLen + client->cfg_file_size_parameter_length],
               fileSizeCompressed, client->cfg_file_size_parameter_length);
        break;
    case UDS_MOOP_DELFILE: // 2
        bufSizeRequired = 4 + n_filePathLen + 1;
        if (bufSizeRequired > sizeof(client->send_buf)) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[4], filePath, n_filePathLen); // filePathAndName
        break;
    case UDS_MOOP_REPLFILE: // 3
        bufSizeRequired = 4 + n_filePathLen + 2 + 2 * client->cfg_file_size_parameter_length;
        if (bufSizeRequired > sizeof(client->send_buf)) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[4], filePath, n_filePathLen); // filePathAndName
        client->send_buf[4 + n_filePathLen] = client->cfg_data_format_identifier;
        client->send_buf[5 + n_filePathLen] = client->cfg_file_size_parameter_length;
        PackBE(&client->send_buf[6 + n_filePathLen], fileSizeUncompressed,
               client->cfg_file_size_parameter_length);
        PackBE(&client->send_buf[6 + n_filePathLen + client->cfg_file_size_parameter_length],
               fileSizeCompressed, client->cfg_file_size_parameter_length);
        break;
    case UDS_MOOP_RDFILE: // 4
        bufSizeRequired = 4 + n_filePathLen + 1;
        if (bufSizeRequired > sizeof(client->send_buf)) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[4], filePath, n_filePathLen); // filePathAndName
        client->send_buf[4 + n_filePathLen] = client->cfg_data_format_identifier;
        break;
    case UDS_MOOP_RDDIR: // 5
        bufSizeRequired = 4 + n_filePathLen;
        if (bufSizeRequired > sizeof(client->send_buf)) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[4], filePath, n_filePathLen); // filePathAndName
        break;
    case UDS_MOOP_RSFILE: // 6
        bufSizeRequired = 4 + n_filePathLen + 2 + 2 * client->cfg_file_size_parameter_length;
        if (bufSizeRequired > sizeof(client->send_buf)) {
            return UDS_ERR_BUFSIZ;
        }
        memmove(&client->send_buf[4], filePath, n_filePathLen); // filePathAndName
        client->send_buf[4 + n_filePathLen] = client->cfg_data_format_identifier;
        client->send_buf[5 + n_filePathLen] = client->cfg_file_size_parameter_length;
        PackBE(&client->send_buf[6 + n_filePathLen], fileSizeUncompressed,
               client->cfg_file_size_parameter_length);
        PackBE(&client->send_buf[6 + n_filePathLen + client->cfg_file_size_parameter_length],
               fileSizeCompressed, client->cfg_file_size_parameter_length);
        break;
    default:
        UDS_ASSERT(0);
        break;
    }
    // Phew!

    client->send_size = (uint16_t)bufSizeRequired;
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
 * @brief Validates and parses server's response to RequestDownload.
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
    uint8_t mnrobSize = (client->recv_buf[1] & 0xF0) >> 4;
    UDS_ASSERT(mnrobSize <= 15);

    if (client->recv_size < 2 + mnrobSize) {
        return UDS_ERR_RESP_TOO_SHORT;
    }

    UDSErr_t err =
        UnpackBEu32(&client->recv_buf[UDS_0X34_RESP_BASE_LEN], &resp->maxBlockLength, mnrobSize);
    if (err) {
        return err;
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
    if (r->recv_len < UDS_0X11_REQ_MIN_LEN) {
        return NegativeResponse(r, UDS_NRC_IncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t resetType = r->recv_buf[1] & 0x3F;

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
 * @brief decodes addressAndLength at a nonzero offset within the receive buffer.
 * @see ISO 14229-1:2020(E) Annex H
 *
 * @param srv
 * @param buf pointer to addressAndDataLengthFormatIdentifier in recv_buf
 * @param memoryAddress the decoded memory address
 * @param memorySize the decoded memory size
 * @param offset how many elements (addres and size pairs) away from the format identifier
 * @return uint8_t
 */
static UDSErr_t decodeAddressAndLengthAt(UDSReq_t *r, uint8_t *const buf, void **memoryAddress,
                                         size_t *memorySize, size_t offset) {
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

    UDSErr_t err = UnpackBEuintptr(&buf[1 + offsetBytes], &tmp, memoryAddressLength);
    if (err) {
        return err;
    }
    *memoryAddress = (void *)tmp;

    err = UnpackBEsize(&buf[1 + offsetBytes + memoryAddressLength], memorySize, memorySizeLength);
    if (err) {
        return err;
    }

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
static UDSErr_t decodeAddressAndLength(UDSReq_t *r, uint8_t *const buf, void **memoryAddress,
                                       size_t *memorySize) {
    return decodeAddressAndLengthAt(r, buf, memoryAddress, memorySize, 0);
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

    BeginTransfer(srv, memorySize, args.maxNumberOfBlockLength);

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

    BeginTransfer(srv, memorySize, args.maxNumberOfBlockLength);

    uint8_t lengthFormatIdentifier = (uint8_t)(sizeof(args.maxNumberOfBlockLength) << 4);

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_UPLOAD);
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
        err = UDS_NRC_ConditionsNotCorrect;
        goto done;
    }
    if (r->recv_len < UDS_0X38_REQ_BASE_LEN) {
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto done;
    }

    const uint8_t mode_of_operation = r->recv_buf[1];

    switch (mode_of_operation) {
    case UDS_MOOP_ADDFILE:
    case UDS_MOOP_DELFILE:
    case UDS_MOOP_REPLFILE:
    case UDS_MOOP_RDFILE:
    case UDS_MOOP_RDDIR:
    case UDS_MOOP_RSFILE:
        break;
    default:
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto done;
    }

    const uint16_t file_path_len = (uint16_t)((r->recv_buf[2] << 8) + (r->recv_buf[3]));
    uint8_t data_format_identifier = 0;
    uint8_t file_size_parameter_length = 0; // also called "k" in ISO14229:2020
    size_t file_size_uncompressed = 0;
    size_t file_size_compressed = 0;
    size_t byte_idx = 4 + file_path_len;

    if (byte_idx > r->recv_len) {
        err = UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        goto done;
    }

    if (mode_of_operation == UDS_MOOP_DELFILE || mode_of_operation == UDS_MOOP_RDDIR) {
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
        if ((size_t)byte_idx + 2 * file_size_parameter_length > r->recv_len) {
            err = UDS_NRC_RequestOutOfRange;
            goto done;
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
        .filePath = file_path_len == 0 ? NULL : &r->recv_buf[4],
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

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_FILE_TRANSFER);
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
    r->send_buf[r->send_len] = UDS_MOOP_RDDIR ? 0x00 : args.dataFormatIdentifier;
    r->send_len += 1;

    if (mode_of_operation == UDS_MOOP_ADDFILE || mode_of_operation == UDS_MOOP_DELFILE ||
        mode_of_operation == UDS_MOOP_REPLFILE || mode_of_operation == UDS_MOOP_RSFILE) {
        // pass
    } else {
        // fileSizeOrDirInfoParameterLength
        PackBE(&r->send_buf[r->send_len], sizeof(args.fileSizeUnCompressed), 2);
        r->send_len += 2;

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

    if (mode_of_operation == UDS_MOOP_ADDFILE || mode_of_operation == UDS_MOOP_DELFILE ||
        mode_of_operation == UDS_MOOP_REPLFILE || mode_of_operation == UDS_MOOP_RDFILE ||
        mode_of_operation == UDS_MOOP_RDDIR) {
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
    r->send_buf[1] = r->recv_buf[1]; /* do not use `type` because we want to preserve the suppress
                                        response bit */
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

/// signature of internal service handlers
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

            if (r->send_len) {
                UDSErr_t err = UDS_OK;
                err = UDSTpSend(srv->tp, r->send_buf, r->send_len, NULL);
                if (UDS_OK != err) {
                    EmitEvent(srv, UDS_EVT_Err, &err);
                    UDS_LOGE(__FILE__, "UDSTpSend failed with %s", UDSErrToStr(err));
                }
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
        size_t recvlen = 0;
        UDSErr_t err = UDSTpRecv(srv->tp, r->recv_buf, sizeof(r->recv_buf), &recvlen, &r->info);
        if (UDS_OK != err) {
            UDS_LOGE(__FILE__, "UDSTpRecv failed with %s\n", UDSErrToStr(err));
            return;
        }
        r->recv_len = recvlen;

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




UDSErr_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, const size_t len, const UDSSDU_t *info) {
    if (NULL == hdl || NULL == hdl->send) {
        return UDS_ERR_INVALID_ARG;
    }
    return hdl->send(hdl, (uint8_t *)buf, len, info);
}

UDSErr_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, const size_t bufsiz, size_t *recvlen,
                       UDSSDU_t *info) {
    if (NULL == hdl || NULL == hdl->recv || NULL == recvlen) {
        return UDS_ERR_INVALID_ARG;
    }
    return hdl->recv(hdl, buf, bufsiz, recvlen, info);
}

void UDSTpPoll(UDSTp_t *hdl) {
    if (NULL == hdl || NULL == hdl->poll) {
        return;
    }
    hdl->poll(hdl);
}


#ifdef UDS_LINES
#line 1 "src/util.c"
#endif





#if defined(UDS_CUSTOM_MILLIS)
// the user is expected to provide a UDSMillis implementation
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
#elif UDS_SYS == UDS_SYS_ZEPHYR
    return k_uptime_get_32();
#else
#error "UDSMillis not implemented for this UDS_SYS"
#endif
}
#endif // defined(UDS_CUSTOM_MILLIS)

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

void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer, size_t buflen,
                        const UDSSDU_t *info) {
    (void)info;
    for (size_t i = 0; i < buflen; i++) {
        UDS_LogWrite(level, tag, "%02x ", buffer[i]);
    }
    UDS_LogWrite(level, tag, "\n");
}
#endif


#ifdef UDS_LINES
#line 1 "src/tp/isotp_c.c"
#endif
#if defined(UDS_TP_ISOTP_C)








static void tp_poll(UDSTp_t *hdl) {
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    isotp_poll(&impl->func_link);
    if (ISOTP_SEND_STATUS_INPROGRESS == impl->phys_link.send_status) {
        hdl->status.is_sending = 1;
    } else {
        hdl->status.is_sending = 0;
    }
}

static UDSErr_t tp_send(UDSTp_t *hdl, const uint8_t *buf, size_t len, const UDSSDU_t *info) {
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    const UDS_A_TA_Type_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    if (len > UINT32_MAX) {
        return UDS_FAIL;
    }

    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &tp->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &tp->func_link;
        if (len > 7) {
            UDS_LOGE(__FILE__, "Cannot send more than 7 bytes via functional addressing");
            return UDS_ERR_MISUSE;
        }
        break;
    default:
        UDS_LOGE(__FILE__, "unknown UDS_A_TA_TYPE");
        return UDS_ERR_MISUSE;
    }

    int ret = isotp_send(link, buf, (uint32_t)len);
    switch (ret) {
    case ISOTP_RET_OK: {
        return UDS_OK;
    }
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        return UDS_ERR_TPORT;
    }
}

static inline UDSErr_t safe_api_shim_isotp_receive(
    IsoTpLink* link, 
    uint8_t* payload, 
    const size_t payload_size, // size of payload buffer
    size_t* out_size,
    int *isotp_ret
) {
    if (payload_size > sizeof(uint32_t)) { // sizeof(isotp_receive payload_size) arg
        return UDS_FAIL;
    }
    if (sizeof(*out_size) > sizeof(uint32_t)) { // sizeof(isotp_receive *out_size) arg
        return UDS_FAIL;
    }
    uint32_t u32out_size = 0;
    *isotp_ret = isotp_receive(link, payload, (uint32_t)payload_size, &u32out_size) ;

    if (u32out_size > sizeof(*out_size)) {
        return UDS_FAIL;
    }
    *out_size = u32out_size;
    return UDS_OK;
}

static UDSErr_t tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsiz, size_t *recvlen,
                            UDSSDU_t *info) {
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    int ret = 0;
    UDSErr_t err = UDS_OK;

    err = safe_api_shim_isotp_receive(&tp->phys_link, buf, bufsiz, recvlen, &ret);
    if (UDS_OK != err) {
        goto done;
    }
    if (ISOTP_RET_OK == ret) {
        UDS_LOGI(__FILE__, "phys link received %zd bytes", *recvlen);
        if (NULL != info) {
            info->A_TA = tp->phys_sa;
            info->A_SA = tp->phys_ta;
            info->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
        }
    } else if (ISOTP_RET_NO_DATA == ret) {
        err = safe_api_shim_isotp_receive(&tp->func_link, buf, bufsiz, recvlen, &ret);
        if (UDS_OK != err) {
            goto done;
        }
        if (ISOTP_RET_OK == ret) {
            UDS_LOGI(__FILE__, "func link received %zd bytes", *recvlen);
            if (NULL != info) {
                info->A_TA = tp->func_sa;
                info->A_SA = tp->func_ta;
                info->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
            }
        } else if (ISOTP_RET_NO_DATA == ret) {
            goto done;
        } else {
            UDS_LOGE(__FILE__, "unhandled return code from func link %d\n", ret);
        }
    } else {
        UDS_LOGE(__FILE__, "unhandled return code from phys link %d\n", ret);
    }
    done:
    return err;
}

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t sa, uint32_t ta, uint32_t sa_func,
                                uint32_t ta_func) {
    if (tp == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    tp->hdl.poll = tp_poll;
    tp->hdl.send = tp_send;
    tp->hdl.recv = tp_recv;
    tp->phys_sa = sa;
    tp->phys_ta = ta;
    tp->func_sa = sa_func;
    tp->func_ta = ta_func;

    isotp_init_link(&tp->phys_link, tp->phys_ta, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
    isotp_init_link(&tp->func_link, tp->func_ta, tp->func_send_buf, sizeof(tp->func_send_buf),
                    tp->func_recv_buf, sizeof(tp->func_recv_buf));
    return UDS_OK;
}

UDSErr_t UDSServerTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t source_addr, uint32_t target_addr,
                               uint32_t source_addr_func) {
    return UDSTpISOTpCInit(tp, source_addr, target_addr, source_addr_func, UDS_TP_NOOP_ADDR);
}

UDSErr_t UDSClientTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t target_addr, uint32_t source_addr,
                               uint32_t target_addr_func) {
    return UDSTpISOTpCInit(tp, source_addr, target_addr, UDS_TP_NOOP_ADDR, target_addr_func);
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

static void SocketCANRecv(UDSTpISOTpCSocketCAN_t *tp) {
    UDS_ASSERT(tp);
    struct can_frame frame = {0};
    ssize_t nbytes = 0;

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
            if (frame.can_id == tp->hdl2.phys_sa) {
                isotp_on_can_message(&tp->hdl2.phys_link, frame.data, frame.can_dlc);
            } else if (frame.can_id == tp->hdl2.func_sa) {
                if (ISOTP_RECEIVE_STATUS_IDLE != tp->hdl2.phys_link.receive_status) {
                    UDS_LOGI(__FILE__,
                             "func frame received but cannot process because link is not idle");
                    return;
                }
                // TODO: reject if it's longer than a single frame
                isotp_on_can_message(&tp->hdl2.func_link, frame.data, frame.can_dlc);
            }
        }
    }
}


UDSErr_t UDSTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                  uint32_t source_addr, uint32_t target_addr,
                                  uint32_t source_addr_func, uint32_t target_addr_func) {
    UDSErr_t err = UDS_OK;

    UDSTpISOTpCInit(&tp->hdl2, source_addr, target_addr, source_addr_func, target_addr_func);
    if (err) {
        return err;
    }
    tp->fd = SetupSocketCAN(ifname);
    tp->hdl2.phys_link.user_send_can_arg = &(tp->fd);
    tp->hdl2.func_link.user_send_can_arg = &(tp->fd);

    return UDS_OK;
}

UDSErr_t UDSServerTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                  uint32_t source_addr, uint32_t target_addr,
                                  uint32_t source_addr_func) {
    return UDSTpISOTpCSocketCANInit(tp, ifname, source_addr, target_addr, source_addr_func, UDS_TP_NOOP_ADDR);
}

UDSErr_t UDSClientTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                  uint32_t target_addr, uint32_t source_addr,
                                  uint32_t target_addr_func) {
    return UDSTpISOTpCSocketCANInit(tp, ifname, source_addr, target_addr, UDS_TP_NOOP_ADDR, target_addr_func);
                                }

void UDSTpISOTpCSocketCANDeinit(UDSTpISOTpCSocketCAN_t *tp) {
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

static void isotp_sock_tp_poll(UDSTp_t *hdl) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
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
                        break;
                    default:
                        UDS_LOGE(__FILE__, "Asynchronous socket error: %s (%d)",
                                 strerror(pending_err), pending_err);
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
                    hdl->status.is_sending = 1;
                } else {
                    hdl->status.is_sending = 0;
                }
            }
        }
    }
}

static UDSErr_t tp_recv_once(int fd, uint8_t *buf, const size_t bufsiz, size_t *recvlen) {
    UDSErr_t err = UDS_OK;
    ssize_t ret = read(fd, buf, bufsiz);
    if (ret < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            ; // temporarily unavailable -- not an error
        } else {
            UDS_LOGE(__FILE__, "read failed: %zd with errno: %d", ret, errno);
            err = UDS_FAIL;
        }
    }

    *recvlen = ret < 0 ? 0 : (size_t)ret;
    return err;
}

static UDSErr_t isotp_sock_tp_recv(UDSTp_t *hdl, uint8_t *buf, const size_t bufsiz, size_t *recvlen, UDSSDU_t *info) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    UDSErr_t err = 0;
    UDSSDU_t *msg = &impl->recv_info;

    err = tp_recv_once(impl->phys_fd, buf, bufsiz, recvlen);
    if (err) {
        return err;
    }
    if (*recvlen > 0) {
        msg->A_TA = impl->phys_sa;
        msg->A_SA = impl->phys_ta;
        msg->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
    } else {
        err = tp_recv_once(impl->func_fd, buf, bufsiz, recvlen);
        if (err) {
            return err;
        }
        if (*recvlen > 0) {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
            msg->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
        }
    }

    if (*recvlen > 0) {
        if (info) {
            *info = *msg;
        }

        UDS_LOGD(__FILE__, "'%s' received %zd bytes from 0x%03x (%s), ", impl->tag, *recvlen,
                 msg->A_TA, msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        UDS_LOG_SDU(__FILE__, impl->recv_buf, *recvlen, msg);
    }
    return UDS_OK;
}

static UDSErr_t isotp_sock_tp_send(UDSTp_t *hdl, const uint8_t *buf, const size_t len,
                                       const UDSSDU_t *info) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    ssize_t ret = -1;
    int fd = -1;
    const UDS_A_TA_Type_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    switch (ta_type) {
        case UDS_A_TA_TYPE_PHYSICAL:
        fd = impl->phys_fd;
        break;
        case UDS_A_TA_TYPE_FUNCTIONAL: {
            if (len > 7) {
                UDS_LOGE(__FILE__, "UDSTpIsoTpSock: functional request too large");
                return UDS_ERR_MISUSE;
            }
            fd = impl->func_fd;
        }
        break;
    default:
        UDS_LOGE(__FILE__, "unknown UDS_A_TA_TYPE");
        return UDS_ERR_MISUSE;
    }

    UDS_ASSERT(fd >= 0);
    ret = write(fd, buf, len);
    UDS_ASSERT(ret < UINT16_MAX);
    if (ret < 0) {
        perror("write");
        return UDS_FAIL;
    }

    uint32_t ta = ta_type == UDS_A_TA_TYPE_PHYSICAL ? impl->phys_ta : impl->func_ta;
    UDS_LOGD(__FILE__, "'%s' sends %zu bytes to 0x%03x (%s)", impl->tag, len, ta,
             ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    UDS_LOG_SDU(__FILE__, buf, len, info);
    return UDS_OK;
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
        UDS_LOGI(__FILE__, "Bind: %s %s", strerror(errno), if_name);
        return -1;
    }
    return fd;
}

UDSErr_t UDSServerTpIsoTpSockInit(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
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
    if (tp->phys_fd < 0) {
        return UDS_FAIL;
    }
    tp->func_fd = LinuxSockBind(ifname, source_addr_func, 0, true);
    if (tp->func_fd < 0) {
        return UDS_FAIL;
    }
    const char *tag = "server";
    memmove(tp->tag, tag, strlen(tag));
    UDS_LOGI(__FILE__, "%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x",
             strlen(tp->tag) ? tp->tag : "server", source_addr, target_addr, source_addr_func,
             target_addr);
    return UDS_OK;
}

UDSErr_t UDSClientTpIsoTpSockInit(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
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

/// \cond INTERNAL_INTERFACE



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

static UDSErr_t mock_tp_send(struct UDSTp *hdl, const uint8_t *buf, size_t len,
                                 const UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl;
    if (MsgCount >= NUM_MSGS) {
        UDS_LOGW(__FILE__, "mock_tp_send: too many messages in the queue");
        return UDS_FAIL;
    }
    struct Msg *m = &msgs[MsgCount++];
    UDS_A_TA_Type_t ta_type =
        info == NULL ? (UDS_A_TA_Type_t)UDS_A_TA_TYPE_PHYSICAL : (UDS_A_TA_Type_t)info->A_TA_Type;
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
            UDS_LOGW(__FILE__, "mock_tp_send: functional message too long: %zu", len);
            return UDS_FAIL;
        }
        m->info.A_TA = tp->ta_func;
        m->info.A_SA = tp->sa_func;
    } else {
        UDS_LOGW(__FILE__, "mock_tp_send: unknown TA type: %d", ta_type);
        return UDS_FAIL;
    }
    m->info.A_TA_Type = ta_type;
    m->scheduled_tx_time = UDSMillis() + tp->send_tx_delay_ms;
    memmove(m->buf, buf, len);

    UDS_LOGD(__FILE__, "%s sends %ld bytes to TA=0x%03X (A_TA_Type=%s):", tp->name, len,
             m->info.A_TA, m->info.A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "PHYSICAL" : "FUNCTIONAL");
    UDS_LOG_SDU(__FILE__, buf, len, &m->info);

    return UDS_OK;
}

static UDSErr_t mock_tp_recv(struct UDSTp *hdl, uint8_t *buf, size_t bufsiz, size_t *recvlen, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl;
    if (tp->recv_len == 0) {
        return UDS_OK;
    }
    if (bufsiz < tp->recv_len) {
        UDS_LOGE(__FILE__, "mock_tp_recv: buffer too small: %ld < %ld", bufsiz, tp->recv_len);
        return UDS_FAIL;
    }
    *recvlen = tp->recv_len;
    memmove(buf, tp->recv_buf, tp->recv_len);
    if (info) {
        *info = tp->recv_info;
    }
    tp->recv_len = 0;
    return UDS_OK;
}

static void mock_tp_poll(struct UDSTp *hdl) {
    (void)hdl; // unused parameter
    NetworkPoll();
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

/// \endcond INTERNAL_INTERFACE

#endif

#if defined(UDS_TP_ISOTP_C)
/// \cond DOXYGEN_SHOULD_SKIP_THIS

#ifndef ISO_TP_USER_SEND_CAN_ARG
#error
#endif

#ifdef UDS_LINES
#line 1 "src/tp/isotp-c/isotp.c"
#endif
////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include <stdint.h>



///////////////////////////////////////////////////////
///                 STATIC FUNCTIONS                ///
///////////////////////////////////////////////////////

/* CAN frame data lengths (CAN_DL) which may be used by CAN FD frames larger than
 * a Classical CAN frame. CAN FD frames can only carry these lengths, so frames
 * exceeding 8 bytes always have to be padded up to the next valid length.
 */
static const uint8_t isotp_can_fd_frame_sizes[] = {12, 16, 20, 24, 32, 48, 64};

/* Returns the smallest valid CAN_DL which is able to carry length bytes of data */
static uint8_t isotp_ceil_can_dl(uint8_t length) {
    if (length <= ISOTP_CAN_DL_CLASSIC) {
        /* every length up to 8 bytes maps directly to a DLC */
        return length;
    }

    for (uint8_t i = 0; i < (uint8_t)(sizeof(isotp_can_fd_frame_sizes) / sizeof(isotp_can_fd_frame_sizes[0])); ++i) {
        if (length <= isotp_can_fd_frame_sizes[i]) { return isotp_can_fd_frame_sizes[i]; }
    }

    return isotp_can_fd_frame_sizes[sizeof(isotp_can_fd_frame_sizes) / sizeof(isotp_can_fd_frame_sizes[0]) - 1];
}

/* Returns logic true if length is a CAN_DL which can be transmitted on a CAN(-FD) bus */
static uint8_t isotp_is_valid_can_dl(uint8_t length) { return (uint8_t)(length == isotp_ceil_can_dl(length)); }

/* Returns the TX_DL to use for the given link, falling back to Classical CAN for
 * links which weren't initialised through isotp_init_link()
 */
static uint8_t isotp_tx_dl(const IsoTpLink* link) {
    if (link->tx_dl < ISOTP_CAN_DL_CLASSIC || link->tx_dl > ISO_TP_MAX_CAN_FRAME_SIZE || !isotp_is_valid_can_dl(link->tx_dl)) { return ISOTP_CAN_DL_CLASSIC; }

    return link->tx_dl;
}

#ifdef ISO_TP_USER_SEND_CAN_FLAGS
/* Returns the frame format flags the user shim has to transmit the frames of this link with.
 * Links using a TX_DL of more than 8 bytes require CAN FD frames, so all of their frames are
 * flagged accordingly, irrespective of the length of the individual frame.
 */
static uint8_t isotp_frame_flags(const IsoTpLink* link) {
    uint8_t flags = ISOTP_CAN_FRAME_FLAG_NONE;

    if (isotp_tx_dl(link) > ISOTP_CAN_DL_CLASSIC) {
        flags |= ISOTP_CAN_FRAME_FLAG_FD;

    #ifdef ISO_TP_CAN_FD_USE_BRS
        flags |= ISOTP_CAN_FRAME_FLAG_BRS;
    #endif
    }

    return flags;
}
#endif

/* Pads a frame containing used_length bytes up to a transmittable CAN_DL and
 * returns the resulting frame length.
 *
 * Frames of more than 8 bytes are always padded, as CAN FD only supports a
 * discrete set of frame lengths. Smaller frames are only padded if
 * ISO_TP_FRAME_PADDING is enabled.
 */
static uint8_t isotp_pad_frame(IsoTpCanMessage* message, uint8_t used_length) {
    uint8_t frame_length = used_length;

#ifdef ISO_TP_FRAME_PADDING
    if (frame_length < ISOTP_CAN_DL_CLASSIC) { frame_length = ISOTP_CAN_DL_CLASSIC; }
#endif

    frame_length = isotp_ceil_can_dl(frame_length);

    if (frame_length > used_length) { (void)memset(message->as.data_array.ptr + used_length, ISO_TP_FRAME_PADDING_VALUE, frame_length - used_length); }

    return frame_length;
}

/* st_min to microsecond */
static uint8_t isotp_us_to_st_min(uint32_t us) {
    // ISO 15765-2:2016 defines STmin encoding:
    // 0x00..0x7F: value in milliseconds (0..127 ms)
    // 0xF1..0xF9: value in 100 microsecond steps (100..900 us)
    const uint32_t STMIN_MS_MAX = 127000;      // 127 ms in us
    const uint32_t STMIN_US_MIN = 100;         // 100 us
    const uint32_t STMIN_US_MAX = 900;         // 900 us
    const uint8_t  STMIN_US_BASE = 0xF0;       // base for 100us steps

    if (us <= STMIN_MS_MAX) {
        if (us >= STMIN_US_MIN && us <= STMIN_US_MAX) {
            return (uint8_t)(STMIN_US_BASE + (us / 100));
        } else {
            return (uint8_t)(us / 1000u);
        }
    }

    return 0;
}

/* st_min to usec  */
static uint32_t isotp_st_min_to_us(uint8_t st_min) {
    // ISO 15765-2:2016 defines STmin encoding:
    // 0x00..0x7F: value in milliseconds (0..127 ms)
    // 0xF1..0xF9: value in 100 microsecond steps (100..900 us)
    const uint8_t  STMIN_MS_MAX      = 0x7F;   // 127 ms
    const uint8_t  STMIN_US_MIN_CODE = 0xF1;   // 100 us
    const uint8_t  STMIN_US_MAX_CODE = 0xF9;   // 900 us
    const uint8_t  STMIN_US_BASE     = 0xF0;   // base for 100us steps
    const uint32_t US_PER_MS         = 1000;
    const uint32_t US_STEP           = 100;

    if (st_min <= STMIN_MS_MAX) {
        return st_min * US_PER_MS;
    } else if (st_min >= STMIN_US_MIN_CODE && st_min <= STMIN_US_MAX_CODE) {
        return (st_min - STMIN_US_BASE) * US_STEP;
    }
    return 0;
}

static int isotp_send_flow_control(const IsoTpLink* link, uint8_t flow_status, uint8_t block_size, uint32_t st_min_us) {
    IsoTpCanMessage message;
    (void)memset(&message, 0, sizeof(message));
    int             ret;
    uint8_t         size = 0;

    /* setup message  */
    message.as.flow_control.type  = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.FS    = flow_status;
    message.as.flow_control.BS    = block_size;
    message.as.flow_control.STmin = isotp_us_to_st_min(st_min_us);

    /* send message */
    size = isotp_pad_frame(&message, 3);

    ret = isotp_user_send_can(link->send_arbitration_id, message.as.data_array.ptr, size
#if defined(ISO_TP_USER_SEND_CAN_FLAGS)
                              , isotp_frame_flags(link)
#endif
#if defined(ISO_TP_USER_SEND_CAN_ARG)
                              , link->user_send_can_arg
#endif
    );

    return ret;
}

static int isotp_send_single_frame(const IsoTpLink* link, uint32_t id) {
    (void)id; // Prevent unused variable warning

    IsoTpCanMessage message;
    int             ret;
    uint8_t         size = 0;

    (void)memset(&message, 0, sizeof(message));

    /* payloads which don't fit into a single frame must be segmented */
    assert(link->send_size <= ISOTP_SF_MAX_PAYLOAD(isotp_tx_dl(link)));

    /* setup message  */
    if (link->send_size < ISOTP_CAN_DL_CLASSIC) {
        message.as.single_frame.type  = ISOTP_PCI_TYPE_SINGLE;
        message.as.single_frame.SF_DL = (uint8_t)link->send_size;
        (void)memcpy(message.as.single_frame.data, link->send_buffer, link->send_size);

        size = isotp_pad_frame(&message, (uint8_t)(link->send_size + 1u));
    } else { // ISO15765-2:2016, CAN FD only
        /* setup message using the SF_DL escape sequence */
        message.as.single_frame_escape.type        = ISOTP_PCI_TYPE_SINGLE;
        message.as.single_frame_escape.set_to_zero = 0;
        message.as.single_frame_escape.SF_DL       = (uint8_t)link->send_size;
        (void)memcpy(message.as.single_frame_escape.data, link->send_buffer, link->send_size);

        size = isotp_pad_frame(&message, (uint8_t)(link->send_size + 2u));
    }

    /* send message */
    ret = isotp_user_send_can(link->send_arbitration_id, message.as.data_array.ptr, size
#if defined(ISO_TP_USER_SEND_CAN_FLAGS)
                              , isotp_frame_flags(link)
#endif
#if defined(ISO_TP_USER_SEND_CAN_ARG)
                              , link->user_send_can_arg
#endif
    );

    return ret;
}
static int isotp_send_first_frame(IsoTpLink* link, uint32_t id) {
    IsoTpCanMessage message;
    int             ret   = 0;
    const uint8_t   tx_dl = isotp_tx_dl(link);
    uint32_t        data_length;

    (void)memset(&message, 0, sizeof(message));

    /* payloads which fit into a single frame must not be segmented */
    assert(link->send_size > ISOTP_SF_MAX_PAYLOAD(tx_dl));

    /* first frames always use the full frame length of the sender (TX_DL) */
    if (link->send_size <= 4095) {
        /* setup 'short' message */
        data_length                             = (uint32_t)tx_dl - 2u;
        message.as.first_frame_short.type       = ISOTP_PCI_TYPE_FIRST_FRAME;
        message.as.first_frame_short.FF_DL_low  = (uint8_t)link->send_size;
        message.as.first_frame_short.FF_DL_high = (uint8_t)(0x0F & (link->send_size >> 8));
        (void)memcpy(message.as.first_frame_short.data, link->send_buffer, data_length);
    } else { // ISO15765-2:2016
        /* setup 'long' message */
        data_length                                  = (uint32_t)tx_dl - 6u;
        message.as.first_frame_long.set_to_zero_high = 0;
        message.as.first_frame_long.set_to_zero_low  = 0;
        message.as.first_frame_long.type             = ISOTP_PCI_TYPE_FIRST_FRAME;
        message.as.first_frame_long.FF_DL            = LE32TOH(link->send_size);
        (void)memcpy(message.as.first_frame_long.data, link->send_buffer, data_length);
    }

    /* send message */
    ret = isotp_user_send_can(id, message.as.data_array.ptr, tx_dl
#if defined(ISO_TP_USER_SEND_CAN_FLAGS)
                              , isotp_frame_flags(link)
#endif
#if defined(ISO_TP_USER_SEND_CAN_ARG)
                              , link->user_send_can_arg
#endif
    );

    if (ISOTP_RET_OK == ret) { link->send_offset += data_length; }

    link->send_sn = 1;

    return ret;
}

static int isotp_send_consecutive_frame(IsoTpLink* link) {
    IsoTpCanMessage message;
    uint32_t        data_length;
    uint32_t        max_data_length;
    int             ret;
    uint8_t         size = 0;

    (void)memset(&message, 0, sizeof(message));

    /* payloads which fit into a single frame must not be segmented */
    assert(link->send_size > ISOTP_SF_MAX_PAYLOAD(isotp_tx_dl(link)));

    /* setup message  */
    message.as.consecutive_frame.type = ISOTP_PCI_TYPE_CONSECUTIVE_FRAME;
    message.as.consecutive_frame.SN   = link->send_sn;
    max_data_length                   = (uint32_t)isotp_tx_dl(link) - 1u;
    data_length                       = link->send_size - link->send_offset;
    if (data_length > max_data_length) { data_length = max_data_length; }
    (void)memcpy(message.as.consecutive_frame.data, link->send_buffer + link->send_offset, data_length);

    /* send message */
    size = isotp_pad_frame(&message, (uint8_t)(data_length + 1u));

    ret = isotp_user_send_can(link->send_arbitration_id, message.as.data_array.ptr, size
#if defined(ISO_TP_USER_SEND_CAN_FLAGS)
                              , isotp_frame_flags(link)
#endif
#if defined(ISO_TP_USER_SEND_CAN_ARG)
                              , link->user_send_can_arg
#endif
    );

    if (ISOTP_RET_OK == ret) {
        link->send_offset += data_length;
        if (++(link->send_sn) > 0x0F) { link->send_sn = 0; }
    }

    return ret;
}

static int isotp_receive_single_frame(IsoTpLink* link, const IsoTpCanMessage* message, uint8_t len) {
    const uint8_t* payload;
    uint32_t       payload_length;
    uint32_t       max_payload_length;

    if (0 == message->as.single_frame.SF_DL) {
        /* ISO15765-2:2016: CAN FD frames larger than 8 bytes carry SF_DL in the second byte */
        if (len <= ISOTP_CAN_DL_CLASSIC) {
            isotp_user_debug("Single-frame length too small.");
            return ISOTP_RET_LENGTH;
        }

        payload            = message->as.single_frame_escape.data;
        payload_length     = message->as.single_frame_escape.SF_DL;
        max_payload_length = (uint32_t)len - 2u;
    } else {
        payload            = message->as.single_frame.data;
        payload_length     = message->as.single_frame.SF_DL;
        max_payload_length = (uint32_t)len - 1u;
    }

    /* check data length */
    if ((0 == payload_length) || (payload_length > max_payload_length)) {
        isotp_user_debug("Single-frame length too small.");
        return ISOTP_RET_LENGTH;
    }

    if (payload_length > link->receive_buf_size) {
        isotp_user_debug("Single-frame message too large for receiving buffer.");
        return ISOTP_RET_OVERFLOW;
    }

    /* copying data */
    (void)memcpy(link->receive_buffer, payload, payload_length);
    link->receive_size   = payload_length;
    link->receive_offset = link->receive_size;

#ifdef ISO_TP_ENABLE_STREAMING
    link->receive_stream_size       = link->receive_size;
    link->receive_streaming         = 0;
    link->receive_stream_carry_size = 0;
#endif

    return ISOTP_RET_OK;
}

static int isotp_receive_first_frame(IsoTpLink* link, IsoTpCanMessage* message, uint8_t len) {
    const uint8_t* first_frame_data;
    uint8_t        is_long_packet = 0;
    uint32_t       first_frame_data_length;
    uint32_t       payload_length;

    /* first frames are sent using the full frame length of the sender, which
     * determines the frame length of the following consecutive frames (RX_DL)
     */
    if (len < ISOTP_CAN_DL_CLASSIC || !isotp_is_valid_can_dl(len)) {
        isotp_user_debug("First frame should be a full CAN frame of at least 8 bytes in length.");
        return ISOTP_RET_LENGTH;
    }

    /* check data length */
    payload_length = message->as.first_frame_short.FF_DL_high;
    payload_length = (payload_length << 8) + message->as.first_frame_short.FF_DL_low;

    /* if length is ZERO we get a long message > 4095bytes of payload */
    if (payload_length == 0) {
        is_long_packet          = 1;
        payload_length          = LE32TOH(message->as.first_frame_long.FF_DL);
        first_frame_data_length = (uint32_t)len - 6u;
    } else {
        first_frame_data_length = (uint32_t)len - 2u;
    }

    /* should not use multiple frame transmition */
    if (payload_length <= ISOTP_SF_MAX_PAYLOAD(len)) {
        isotp_user_debug("Should not use multiple frame transmission.");
        return ISOTP_RET_LENGTH;
    }

#ifndef ISO_TP_ENABLE_STREAMING
    if (payload_length > link->receive_buf_size) {
        isotp_user_debug("Multi-frame response too large for receiving buffer.");
        return ISOTP_RET_OVERFLOW;
    }
#else
    if (link->receive_buf_size == 0) {
        isotp_user_debug("Receiving buffer must not be empty.");
        return ISOTP_RET_OVERFLOW;
    }

    link->receive_streaming         = payload_length > link->receive_buf_size;
    link->receive_stream_size       = 0;
    link->receive_stream_carry_size = 0;
#endif

    /* copying data */
    if (is_long_packet) {
        first_frame_data = message->as.first_frame_long.data;
    } else {
        first_frame_data = message->as.first_frame_short.data;
    }

#ifdef ISO_TP_ENABLE_STREAMING
    if (first_frame_data_length > link->receive_buf_size) {
        (void)memcpy(link->receive_buffer, first_frame_data, link->receive_buf_size);
        link->receive_stream_size       = link->receive_buf_size;
        link->receive_stream_carry_size = (uint8_t)(first_frame_data_length - link->receive_buf_size);
        (void)memcpy(link->receive_stream_carry, first_frame_data + link->receive_buf_size, link->receive_stream_carry_size);
    } else {
        (void)memcpy(link->receive_buffer, first_frame_data, first_frame_data_length);
        link->receive_stream_size = first_frame_data_length;
    }
#else
    (void)memcpy(link->receive_buffer, first_frame_data, first_frame_data_length);
#endif

    link->receive_offset = first_frame_data_length;
    link->receive_size   = payload_length;
    link->receive_sn     = 1;
    link->rx_dl          = len;

    return ISOTP_RET_OK;
}

static int isotp_receive_consecutive_frame(IsoTpLink* link, const IsoTpCanMessage* message, uint8_t len) {
    uint32_t remaining_bytes;
    uint32_t max_data_length;

    /* check sn */
    if (link->receive_sn != message->as.consecutive_frame.SN) { return ISOTP_RET_WRONG_SN; }

    /* consecutive frames use the frame length announced by the first frame (RX_DL) */
    max_data_length = (uint32_t)(link->rx_dl < ISOTP_CAN_DL_CLASSIC ? ISOTP_CAN_DL_CLASSIC : link->rx_dl) - 1u;

    /* check data length */
    remaining_bytes = link->receive_size - link->receive_offset;
    if (remaining_bytes > max_data_length) { remaining_bytes = max_data_length; }
    if (remaining_bytes > (uint32_t)(len - 1)) {
        isotp_user_debug("Consecutive frame too short.");
        return ISOTP_RET_LENGTH;
    }

#ifdef ISO_TP_ENABLE_STREAMING
    if (link->receive_streaming) {
        uint32_t available = link->receive_buf_size - link->receive_stream_size;
        uint32_t copy_size = remaining_bytes < available ? remaining_bytes : available;

        (void)memcpy(link->receive_buffer + link->receive_stream_size, message->as.consecutive_frame.data, copy_size);
        link->receive_stream_size += copy_size;

        link->receive_stream_carry_size = (uint8_t)(remaining_bytes - copy_size);
        if (link->receive_stream_carry_size > 0) {
            (void)memcpy(link->receive_stream_carry, message->as.consecutive_frame.data + copy_size, link->receive_stream_carry_size);
        }
    } else
#endif
    {
        /* copying data */
        (void)memcpy(link->receive_buffer + link->receive_offset, message->as.consecutive_frame.data, remaining_bytes);
    }

    link->receive_offset += remaining_bytes;
    if (++(link->receive_sn) > 0x0F) { link->receive_sn = 0; }

    return ISOTP_RET_OK;
}

static int isotp_receive_flow_control_frame(IsoTpLink* link, IsoTpCanMessage* message, uint8_t len) {
    /* unused args */
    (void)link;
    (void)message;

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

int isotp_send(IsoTpLink* link, const uint8_t payload[], uint32_t size) { return isotp_send_with_id(link, link->send_arbitration_id, payload, size); }

int isotp_send_with_id(IsoTpLink* link, uint32_t id, const uint8_t payload[], uint32_t size) {
    int ret;

    if (link == 0x0) {
        isotp_user_debug("Link is null!");
        return ISOTP_RET_ERROR;
    }

    if (size > link->send_buf_size) {
        isotp_user_debug("Message size too large. Increase ISO_TP_MAX_MESSAGE_SIZE to set a larger buffer\n");

#ifndef ISO_TP_NO_FORMATTED_ERRORS
        char    message[ISOTP_MAX_ERROR_MSG_SIZE] = {0};
        int32_t writtenChars = snprintf(&message[0], ISOTP_MAX_ERROR_MSG_SIZE, "Attempted to send %u bytes; max size is %u!\n", (unsigned int)size,
                                        (unsigned int)link->send_buf_size);

        assert(writtenChars <= ISOTP_MAX_ERROR_MSG_SIZE);
        (void)writtenChars;

        isotp_user_debug(message);
#endif
        return ISOTP_RET_OVERFLOW;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status) {
        isotp_user_debug("Abort previous message, transmission in progress.\n");
        return ISOTP_RET_INPROGRESS;
    }

    /* copy into local buffer */
    link->send_size   = size;
    link->send_offset = 0;
    (void)memcpy(link->send_buffer, payload, size);

    if (link->send_size <= ISOTP_SF_MAX_PAYLOAD(isotp_tx_dl(link))) {
        /* send single frame */
        ret = isotp_send_single_frame(link, id);
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
        if (ret == ISOTP_RET_OK && link->tx_done_cb) { link->tx_done_cb(link, link->send_size, link->tx_done_cb_arg); }
#endif
    } else {
        /* send multi-frame */
        ret = isotp_send_first_frame(link, id);

        /* init multi-frame control flags */
        if (ISOTP_RET_OK == ret) {
            link->send_bs_remain       = 0;
            link->send_st_min_us       = 0;
            link->send_wtf_count       = 0;
            link->send_timer_st        = isotp_user_get_us();
            link->send_timer_bs        = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
            link->send_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            link->send_status          = ISOTP_SEND_STATUS_INPROGRESS;
        }
    }

    return ret;
}

void isotp_on_can_message(IsoTpLink* link, const uint8_t* data, uint8_t len) {
    IsoTpCanMessage message;
    int             ret;

    if (len < 2 || len > ISO_TP_MAX_CAN_FRAME_SIZE) { return; }

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

            if (ISOTP_RET_OVERFLOW == ret) {
                /* update protocol result */
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
            } else if (ISOTP_RET_OK == ret) {
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
                /* change status and send fc frame */
#ifdef ISO_TP_ENABLE_STREAMING
                if (link->receive_streaming && link->receive_stream_size >= link->receive_buf_size) {
                    link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
                } else {
                    link->receive_status = ISOTP_RECEIVE_STATUS_INPROGRESS;
                    link->receive_bs_count = link->receive_streaming ? 1 : ISO_TP_DEFAULT_BLOCK_SIZE;
                    isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN_US);
                }
#else
                link->receive_status   = ISOTP_RECEIVE_STATUS_INPROGRESS;
                link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN_US);
#endif
                /* refresh timer cs */
                link->receive_timer_cr = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
            }

            break;
        }
        case ISOTP_PCI_TYPE_CONSECUTIVE_FRAME: {
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
                link->receive_status          = ISOTP_RECEIVE_STATUS_IDLE;
                break;
            }

            /* if success */
            if (ISOTP_RET_OK == ret) {
                /* refresh timer cs */
                link->receive_timer_cr = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;

                /* receive finished */
                if (link->receive_offset >= link->receive_size
#ifdef ISO_TP_ENABLE_STREAMING
                    || (link->receive_streaming && link->receive_stream_size >= link->receive_buf_size)
#endif
                ) {
                    link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
                } else {
                    /* send fc when bs reaches limit */
                    if (0 == --link->receive_bs_count) {
                        link->receive_bs_count =
#ifdef ISO_TP_ENABLE_STREAMING
                            link->receive_streaming ? 1 :
#endif
                            ISO_TP_DEFAULT_BLOCK_SIZE;
                        isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN_US);
                    }
                }
            }

            break;
        }
        case ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME:
            /* handle fc frame only when sending in progress  */
            if (ISOTP_SEND_STATUS_INPROGRESS != link->send_status) { break; }

            /* handle message */
            ret = isotp_receive_flow_control_frame(link, &message, len);

            if (ISOTP_RET_OK == ret) {
                /* refresh bs timer */
                link->send_timer_bs = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;

                /* overflow */
                if (PCI_FLOW_STATUS_OVERFLOW == message.as.flow_control.FS) {
                    link->send_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                    link->send_status          = ISOTP_SEND_STATUS_ERROR;
                }

                /* wait */
                else if (PCI_FLOW_STATUS_WAIT == message.as.flow_control.FS) {
                    link->send_wtf_count += 1;
                    /* wait exceed allowed count */
                    if (link->send_wtf_count > ISO_TP_MAX_WFT_NUMBER) {
                        link->send_protocol_result = ISOTP_PROTOCOL_RESULT_WFT_OVRN;
                        link->send_status          = ISOTP_SEND_STATUS_ERROR;
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
                    link->send_st_min_us       = message_st_min_us > ISO_TP_DEFAULT_ST_MIN_US
                                                     ? message_st_min_us
                                                     : ISO_TP_DEFAULT_ST_MIN_US; // prefer as much st_min as possible for stability?
                    link->send_wtf_count       = 0;
                }
            }
            break;
        default: break;
    };

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    /* Notify user via callback if registered */
    if (link->receive_status == ISOTP_RECEIVE_STATUS_FULL && link->rx_done_cb != NULL
#ifdef ISO_TP_ENABLE_STREAMING
        && !link->receive_streaming
#endif
    ) {
        link->rx_done_cb(link, link->receive_buffer, link->receive_size, link->rx_done_cb_arg);
        link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    }
#endif
    return;
}

int isotp_receive(IsoTpLink* link, uint8_t* payload, const uint32_t payload_size, uint32_t* out_size) {
    uint32_t copylen;

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    /* If callback is registered, isotp_receive should not be used */
    if (link->rx_done_cb != NULL) { return ISOTP_RET_ERROR; /* Callback mode active, use callback instead */ }
#endif

#ifdef ISO_TP_ENABLE_STREAMING
    if (link->receive_streaming) { return ISOTP_RET_ERROR; }
#endif

    if (ISOTP_RECEIVE_STATUS_FULL != link->receive_status) { return ISOTP_RET_NO_DATA; }

    copylen = link->receive_size;
    if (copylen > payload_size) { copylen = payload_size; }

    memcpy(payload, link->receive_buffer, copylen);
    *out_size            = copylen;

    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;

    return ISOTP_RET_OK;
}

#ifdef ISO_TP_ENABLE_STREAMING
int isotp_receive_streaming(IsoTpLink* link, uint8_t* payload, const uint32_t payload_size, uint32_t* out_size, bool* is_complete) {
    uint32_t copylen;

    if (link == NULL || payload == NULL || out_size == NULL || is_complete == NULL) { return ISOTP_RET_ERROR; }
    if (ISOTP_RECEIVE_STATUS_FULL != link->receive_status) { return ISOTP_RET_NO_DATA; }

    copylen = link->receive_streaming ? link->receive_stream_size : link->receive_size;
    if (payload_size < copylen) { return ISOTP_RET_NOSPACE; }

    (void)memcpy(payload, link->receive_buffer, copylen);
    *out_size    = copylen;
    *is_complete = link->receive_offset >= link->receive_size && link->receive_stream_carry_size == 0;

    if (!link->receive_streaming || *is_complete) {
        link->receive_status    = ISOTP_RECEIVE_STATUS_IDLE;
        link->receive_streaming = 0;
        return ISOTP_RET_OK;
    }

    link->receive_stream_size = link->receive_stream_carry_size;
    if (link->receive_stream_size > link->receive_buf_size) {
        link->receive_stream_size = link->receive_buf_size;
    }
    if (link->receive_stream_size > 0) {
        (void)memcpy(link->receive_buffer, link->receive_stream_carry, link->receive_stream_size);
        link->receive_stream_carry_size -= (uint8_t)link->receive_stream_size;
        if (link->receive_stream_carry_size > 0) {
            (void)memmove(link->receive_stream_carry, link->receive_stream_carry + link->receive_stream_size, link->receive_stream_carry_size);
        }
    }

    if (link->receive_stream_carry_size > 0 || link->receive_offset >= link->receive_size) {
        link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
    } else {
        link->receive_status   = ISOTP_RECEIVE_STATUS_INPROGRESS;
        link->receive_bs_count = 1;
        isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN_US);
        link->receive_timer_cr = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
    }

    return ISOTP_RET_OK;
}
#endif

void isotp_init_link(IsoTpLink* link, uint32_t sendid, uint8_t* sendbuf, uint32_t sendbufsize, uint8_t* recvbuf, uint32_t recvbufsize) {
    memset(link, 0, sizeof(*link));
    link->receive_status      = ISOTP_RECEIVE_STATUS_IDLE;
    link->send_status         = ISOTP_SEND_STATUS_IDLE;
    link->send_arbitration_id = sendid;
    link->send_buffer         = sendbuf;
    link->send_buf_size       = sendbufsize;
    link->receive_buffer      = recvbuf;
    link->receive_buf_size    = recvbufsize;
    link->tx_dl               = ISO_TP_DEFAULT_TX_DL;
    link->rx_dl               = ISOTP_CAN_DL_CLASSIC;

#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
    link->tx_done_cb     = NULL;
    link->tx_done_cb_arg = NULL;
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    link->rx_done_cb     = NULL;
    link->rx_done_cb_arg = NULL;
#endif

    return;
}

int isotp_set_tx_dl(IsoTpLink* link, uint8_t tx_dl) {
    if (link == NULL) {
        isotp_user_debug("Link is null!");
        return ISOTP_RET_ERROR;
    }

    if (tx_dl < ISOTP_CAN_DL_CLASSIC || tx_dl > ISO_TP_MAX_CAN_FRAME_SIZE || !isotp_is_valid_can_dl(tx_dl)) {
#ifndef ISO_TP_NO_FORMATTED_ERRORS
        char    message[ISOTP_MAX_ERROR_MSG_SIZE] = {0};
        int32_t writtenChars =
            snprintf(&message[0], ISOTP_MAX_ERROR_MSG_SIZE, "Invalid TX_DL of %u bytes; must be a CAN frame length between 8 and %u!\n",
                     (unsigned int)tx_dl, (unsigned int)ISO_TP_MAX_CAN_FRAME_SIZE);

        assert(writtenChars <= ISOTP_MAX_ERROR_MSG_SIZE);
        (void)writtenChars;

        isotp_user_debug(message);
#else
        isotp_user_debug("Invalid TX_DL; must be a valid CAN frame length.\n");
#endif
        return ISOTP_RET_ERROR;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status) {
        isotp_user_debug("Cannot change TX_DL while a transmission is in progress.\n");
        return ISOTP_RET_INPROGRESS;
    }

    link->tx_dl = tx_dl;

    return ISOTP_RET_OK;
}

uint8_t isotp_get_tx_dl(const IsoTpLink* link) {
    if (link == NULL) {
        isotp_user_debug("Link is null!");
        return 0;
    }

    return isotp_tx_dl(link);
}

void isotp_destroy_link(IsoTpLink* link) {
    if (link == NULL) { return; }

    // Clear callbacks
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
    link->tx_done_cb     = NULL;
    link->tx_done_cb_arg = NULL;
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    link->rx_done_cb     = NULL;
    link->rx_done_cb_arg = NULL;
#endif

    // Reset link state (optional, but good practice)
    memset(link, 0, sizeof(IsoTpLink));
}

void isotp_poll(IsoTpLink* link) {
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
                if (ISOTP_INVALID_BS != link->send_bs_remain) { link->send_bs_remain -= 1; }
                link->send_timer_bs = isotp_user_get_us() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
                link->send_timer_st = isotp_user_get_us() + link->send_st_min_us;

                /* check if send finish */
                if (link->send_offset >= link->send_size) {
                    link->send_status = ISOTP_SEND_STATUS_IDLE;
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
                    if (link->tx_done_cb != NULL) { link->tx_done_cb(link, link->send_size, link->tx_done_cb_arg); }
#endif
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
            link->send_status          = ISOTP_SEND_STATUS_ERROR;
        }
    }

    /* only polling when operation in progress */
    if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {
        /* check timeout */
        if ((link->receive_timer_cr > 0) && IsoTpTimeAfter(isotp_user_get_us(), link->receive_timer_cr)) {
            link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_CR;
            link->receive_status          = ISOTP_RECEIVE_STATUS_IDLE;
        }
    }

    return;
}

#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
void isotp_set_tx_done_cb(IsoTpLink* link, isotp_tx_done_cb cb, void* arg) {
    if (link != NULL) {
        link->tx_done_cb     = cb;
        link->tx_done_cb_arg = arg;
    }
}
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
void isotp_set_rx_done_cb(IsoTpLink* link, isotp_rx_done_cb cb, void* arg) {
    if (link != NULL) {
        link->rx_done_cb     = cb;
        link->rx_done_cb_arg = arg;
    }
}
#endif

/// \endcond
#endif // if defined(UDS_TP_ISOTP_C)

