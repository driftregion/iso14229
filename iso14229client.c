#include "iso14229client.h"
#include "iso14229.h"
#include "isotp-c/isotp.h"
#include "isotp-c/isotp_defines.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void clearRequestContext(Iso14229Client *client) {
    assert(client);
    assert(client->link);
    client->req = (struct Iso14229Request){
        .buf = client->link->send_buffer,
        .buffer_size = client->link->send_buf_size,
        .len = 0,
    };
    client->resp = (struct Iso14229Response){
        .buf = client->link->receive_buffer,
        .buffer_size = client->link->receive_buf_size,
        .len = 0,
    };
    client->state = kRequestStateIdle;
    client->err = kRequestNoError;
}

void iso14229ClientInit(Iso14229Client *client, const struct Iso14229ClientConfig *cfg) {
    assert(client);
    assert(cfg);
    assert(cfg->link);
    assert(cfg->userGetms);
    assert(cfg->userCANTransmit);
    assert(cfg->userDebug);

    memset(client, 0, sizeof(*client));

    isotp_init_link(cfg->link, cfg->phys_send_id, cfg->link_send_buffer, cfg->link_send_buf_size,
                    cfg->link_receive_buffer, cfg->link_recv_buf_size, cfg->userGetms,
                    cfg->userCANTransmit, cfg->userDebug);

    client->phys_send_id = cfg->phys_send_id;
    client->func_send_id = cfg->func_send_id;
    client->recv_id = cfg->recv_id;
    client->p2_ms = cfg->p2_ms;
    client->p2_star_ms = cfg->p2_star_ms;
    client->link = cfg->link;
    client->userGetms = cfg->userGetms;
    client->userCANRxPoll = cfg->userCANRxPoll;

    client->negativeResponseIsError = true;

    clearRequestContext(client);
}

static enum Iso14229ClientRequestError _SendRequest(Iso14229Client *client) {
    if (client->suppressPositiveResponse) {
        // ISO14229-1:2013 8.2.2 Table 11
        client->req.buf[1] |= 0x80;
    }

    if (client->sendFunctional) {
        if (ISOTP_RET_OK != isotp_send_with_id(client->link, client->func_send_id, client->req.buf,
                                               client->req.len)) {
            return kRequestNotSentTransportError;
        }
    } else {
        if (ISOTP_RET_OK != isotp_send(client->link, client->req.buf, client->req.len)) {
            return kRequestNotSentTransportError;
        }
    }
    client->state = kRequestStateSending;
    return kRequestNoError;
}

#define PRE_REQUEST_CHECK()                                                                        \
    if (kRequestStateIdle != client->state) {                                                      \
        return kRequestNotSentBusy;                                                                \
    }                                                                                              \
    clearRequestContext(client);

enum Iso14229ClientRequestError ECUReset(Iso14229Client *client,
                                         enum Iso14229ECUResetResetType type) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_ECU_RESET;
    req->buf[1] = type;
    req->len = 2;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError DiagnosticSessionControl(Iso14229Client *client,
                                                         enum Iso14229DiagnosticSessionType mode) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_DIAGNOSTIC_SESSION_CONTROL;
    req->buf[1] = mode;
    req->len = 2;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError CommunicationControl(Iso14229Client *client,
                                                     enum Iso14229CommunicationControlType ctrl,
                                                     enum Iso14229CommunicationType comm) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_COMMUNICATION_CONTROL;
    req->buf[1] = ctrl;
    req->buf[2] = comm;
    req->len = 3;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError TesterPresent(Iso14229Client *client) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_TESTER_PRESENT;
    req->buf[1] = 0;
    req->len = 2;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError ReadDataByIdentifier(Iso14229Client *client,
                                                     const uint16_t *didList,
                                                     const uint16_t numDataIdentifiers) {
    PRE_REQUEST_CHECK();
    assert(didList);
    assert(numDataIdentifiers);
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_READ_DATA_BY_IDENTIFIER;
    for (int i = 0; i < numDataIdentifiers; i++) {
        uint16_t offset = 1 + sizeof(uint16_t) * i;
        if (offset + 2 > req->buffer_size) {
            return kRequestNotSentInvalidArgs;
        }
        (req->buf + offset)[0] = (didList[i] & 0xFF00) >> 8;
        (req->buf + offset)[1] = (didList[i] & 0xFF);
    }
    req->len = 1 + (numDataIdentifiers * sizeof(uint16_t));
    return _SendRequest(client);
}

enum Iso14229ClientRequestError WriteDataByIdentifier(Iso14229Client *client,
                                                      uint16_t dataIdentifier, const uint8_t *data,
                                                      uint16_t size) {
    PRE_REQUEST_CHECK();
    assert(data);
    assert(size);
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_WRITE_DATA_BY_IDENTIFIER;
    if (client->link->send_buf_size <= 3 || size > client->link->send_buf_size - 3) {
        return kRequestNotSentBufferTooSmall;
    }
    req->buf[1] = (dataIdentifier & 0xFF00) >> 8;
    req->buf[2] = (dataIdentifier & 0xFF);
    memmove(&req->buf[3], data, size);
    req->len = 3 + size;
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
 * @return enum Iso14229ClientRequestError
 * @addtogroup routineControl_0x31
 */
enum Iso14229ClientRequestError RoutineControl(Iso14229Client *client, enum RoutineControlType type,
                                               uint16_t routineIdentifier, uint8_t *data,
                                               uint16_t size) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_ROUTINE_CONTROL;
    req->buf[1] = type;
    req->buf[2] = routineIdentifier >> 8;
    req->buf[3] = routineIdentifier;
    if (size) {
        assert(data);
        if (size > req->buffer_size - ISO14229_0X31_REQ_MIN_LEN) {
            return kRequestNotSentBufferTooSmall;
        }
        memmove(&req->buf[ISO14229_0X31_REQ_MIN_LEN], data, size);
    } else {
        assert(NULL == data);
    }
    req->len = ISO14229_0X31_REQ_MIN_LEN + size;
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
 * @return enum Iso14229ClientRequestError
 * @addtogroup requestDownload_0x34
 */
enum Iso14229ClientRequestError RequestDownload(Iso14229Client *client,
                                                uint8_t dataFormatIdentifier,
                                                uint8_t addressAndLengthFormatIdentifier,
                                                size_t memoryAddress, size_t memorySize) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    uint8_t numMemorySizeBytes = (addressAndLengthFormatIdentifier & 0xF0) >> 4;
    uint8_t numMemoryAddressBytes = addressAndLengthFormatIdentifier & 0x0F;

    req->buf[0] = kSID_REQUEST_DOWNLOAD;
    req->buf[1] = dataFormatIdentifier;
    req->buf[2] = addressAndLengthFormatIdentifier;

    uint8_t *ptr = &req->buf[ISO14229_0X34_REQ_BASE_LEN];

    for (int i = numMemoryAddressBytes - 1; i >= 0; i--) {
        *ptr = (memoryAddress & (0xFF << (8 * i))) >> (8 * i);
        ptr++;
    }

    for (int i = numMemorySizeBytes - 1; i >= 0; i--) {
        *ptr = (memorySize & (0xFF << (8 * i))) >> (8 * i);
        ptr++;
    }

    req->len = ISO14229_0X34_REQ_BASE_LEN + numMemoryAddressBytes + numMemorySizeBytes;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param blockSequenceCounter
 * @param blockLength
 * @param fd
 * @return enum Iso14229ClientRequestError
 * @addtogroup transferData_0x36
 */
enum Iso14229ClientRequestError TransferData(Iso14229Client *client, uint8_t blockSequenceCounter,
                                             const uint16_t blockLength, const uint8_t *data,
                                             uint16_t size) {
    PRE_REQUEST_CHECK();
    assert(blockLength > 2);         // blockLength must include SID and sequenceCounter
    assert(size + 2 <= blockLength); // data must fit inside blockLength - 2
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_TRANSFER_DATA;
    req->buf[1] = blockSequenceCounter;
    memmove(&req->buf[ISO14229_0X36_REQ_BASE_LEN], data, size);
    ISO14229USERDEBUG("size: %d, blocklength: %d\n", size, blockLength);
    req->len = ISO14229_0X36_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError TransferDataStream(Iso14229Client *client,
                                                   uint8_t blockSequenceCounter,
                                                   const uint16_t blockLength, FILE *fd) {
    PRE_REQUEST_CHECK();
    assert(blockLength > 2); // blockLength must include SID and sequenceCounter
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_TRANSFER_DATA;
    req->buf[1] = blockSequenceCounter;

    uint16_t size = fread(&req->buf[2], 1, blockLength - 2, fd);
    ISO14229USERDEBUG("size: %d, blocklength: %d\n", size, blockLength);
    req->len = ISO14229_0X36_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @return enum Iso14229ClientRequestError
 * @addtogroup requestTransferExit_0x37
 */
enum Iso14229ClientRequestError RequestTransferExit(Iso14229Client *client) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    req->buf[0] = kSID_REQUEST_TRANSFER_EXIT;
    req->len = 1;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param dtcSettingType
 * @param data
 * @param size
 * @return enum Iso14229ClientRequestError
 * @addtogroup controlDTCSetting_0x85
 */
enum Iso14229ClientRequestError ControlDTCSetting(Iso14229Client *client, uint8_t dtcSettingType,
                                                  uint8_t *data, uint16_t size) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    if (0x00 == dtcSettingType || 0x7F == dtcSettingType ||
        (0x03 <= dtcSettingType && dtcSettingType <= 0x3F)) {
        assert(0); // reserved vals
    }
    req->buf[0] = kSID_CONTROL_DTC_SETTING;
    req->buf[1] = dtcSettingType;

    if (NULL == data) {
        assert(size == 0);
    } else {
        assert(size > 0);
        if (size > req->buffer_size - 2) {
            return kRequestNotSentBufferTooSmall;
        }
        memmove(&req->buf[2], data, size);
    }
    req->len = 2 + size;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param level
 * @param data
 * @param size
 * @return enum Iso14229ClientRequestError
 * @addtogroup securityAccess_0x27
 */
enum Iso14229ClientRequestError SecurityAccess(Iso14229Client *client, uint8_t level, uint8_t *data,
                                               uint16_t size) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->req;
    if (Iso14229SecurityAccessLevelIsReserved(level)) {
        return kRequestNotSentInvalidArgs;
    }
    req->buf[0] = kSID_SECURITY_ACCESS;
    req->buf[1] = level;
    if (size) {
        assert(data);
        if (size > req->buffer_size - ISO14229_0X27_REQ_BASE_LEN) {
            return kRequestNotSentBufferTooSmall;
        }
    } else {
        assert(NULL == data);
    }

    memmove(&req->buf[ISO14229_0X27_REQ_BASE_LEN], data, size);
    req->len = ISO14229_0X27_REQ_BASE_LEN + size;
    return _SendRequest(client);
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return enum Iso14229ClientRequestError
 * @addtogroup securityAccess_0x27
 */
enum Iso14229ClientRequestError UnpackSecurityAccessResponse(Iso14229Client *client,
                                                             struct SecurityAccessResponse *resp) {
    assert(client);
    assert(resp);
    if (ISO14229_RESPONSE_SID_OF(kSID_SECURITY_ACCESS) != client->resp.buf[0]) {
        return kRequestErrorResponseSIDMismatch;
    }
    if (client->resp.len < ISO14229_0X27_RESP_BASE_LEN) {
        return kRequestErrorResponseTooShort;
    }
    resp->securityAccessType = client->resp.buf[1];
    resp->securitySeedLength = client->resp.len - ISO14229_0X27_RESP_BASE_LEN;
    resp->securitySeed = resp->securitySeedLength == 0 ? NULL : &client->resp.buf[2];
    return kRequestNoError;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return enum Iso14229ClientRequestError
 * @addtogroup routineControl_0x31
 */
enum Iso14229ClientRequestError UnpackRoutineControlResponse(Iso14229Client *client,
                                                             struct RoutineControlResponse *resp) {
    assert(client);
    assert(resp);
    if (ISO14229_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL) != client->resp.buf[0]) {
        return kRequestErrorResponseSIDMismatch;
    }
    if (client->resp.len < ISO14229_0X31_RESP_MIN_LEN) {
        return kRequestErrorResponseTooShort;
    }
    resp->routineControlType = client->resp.buf[1];
    resp->routineIdentifier = (client->resp.buf[2] << 8) + client->resp.buf[3];
    resp->routineStatusRecordLength = client->resp.len - ISO14229_0X31_RESP_MIN_LEN;
    resp->routineStatusRecord =
        resp->routineStatusRecordLength == 0 ? NULL : &client->resp.buf[ISO14229_0X31_RESP_MIN_LEN];
    return kRequestNoError;
}

/**
 * @brief
 *
 * @param client
 * @param resp
 * @return enum Iso14229ClientRequestError
 * @addtogroup requestDownload_0x34
 */
enum Iso14229ClientRequestError
UnpackRequestDownloadResponse(const struct Iso14229Response *resp,
                              struct RequestDownloadResponse *unpacked) {
    assert(resp);
    assert(unpacked);
    if (ISO14229_RESPONSE_SID_OF(kSID_REQUEST_DOWNLOAD) != resp->buf[0]) {
        return kRequestErrorResponseSIDMismatch;
    }
    if (resp->len < ISO14229_0X34_RESP_BASE_LEN) {
        return kRequestErrorResponseTooShort;
    }
    uint8_t maxNumberOfBlockLengthSize = (resp->buf[1] & 0xF0) >> 4;

    if (sizeof(unpacked->maxNumberOfBlockLength) < maxNumberOfBlockLengthSize) {
        ISO14229USERDEBUG("WARNING: sizeof(maxNumberOfBlockLength) > sizeof(size_t)");
        return kRequestErrorCannotUnpackResponse;
    }
    unpacked->maxNumberOfBlockLength = 0;
    for (int byteIdx = 0; byteIdx < maxNumberOfBlockLengthSize; byteIdx++) {
        uint8_t byte = resp->buf[ISO14229_0X34_RESP_BASE_LEN + byteIdx];
        uint8_t shiftBytes = maxNumberOfBlockLengthSize - 1 - byteIdx;
        unpacked->maxNumberOfBlockLength |= byte << (8 * shiftBytes);
    }
    return kRequestNoError;
}
/**
 * @brief Check that the response is standard
 *
 * @param ctx
 * @return enum Iso14229ClientRequestError
 */
enum Iso14229ClientRequestError _ClientValidateResponse(const Iso14229Client *client) {

    if (0x7F == client->resp.buf[0]) {
        if (client->req.buf[0] != client->resp.buf[1]) {
            ISO14229USERDEBUG("req->buf[0]: %x, client->resp.buf[1]: %x", client->req.buf[0],
                              client->resp.buf[1]);
            return kRequestErrorResponseSIDMismatch;
        } else if (kRequestCorrectlyReceived_ResponsePending == client->resp.buf[2]) {
            return kRequestNoError;
        } else if (client->negativeResponseIsError) {
            return kRequestErrorNegativeResponse;
        } else {
            ;
        }
    } else {
        if (ISO14229_RESPONSE_SID_OF(client->req.buf[0]) != client->resp.buf[0]) {
            ISO14229USERDEBUG("req->buf[0] %x, client->resp.buf[0]: %x", client->req.buf[0],
                              client->resp.buf[0]);
            return kRequestErrorResponseSIDMismatch;
        }
    }

    return kRequestNoError;
}

/**
 * @brief Handle validated server response.
 * Some server responses modify the client behavior. This is where the client
 * gets modified.
 *
 * @param client
 */
static inline void _ClientHandleResponse(Iso14229Client *client) {
    const struct Iso14229Response *resp = &client->resp;
    if (0x7F == resp->buf[0]) {
        if (kRequestCorrectlyReceived_ResponsePending == resp->buf[2]) {
            ISO14229USERDEBUG("got RCRRP, setting p2 timer\n");
            client->p2_timer = client->userGetms() + client->p2_star_ms;
            client->state = kRequestStateSentAwaitResponse;
        }
    } else {
        uint8_t respSid = resp->buf[0];
        switch (ISO14229_REQUEST_SID_OF(respSid)) {
        case kSID_DIAGNOSTIC_SESSION_CONTROL: {
            if (client->resp.len < ISO14229_0X10_RESP_LEN) {
                ISO14229USERDEBUG("Error: SID %x response too short\n",
                                  kSID_DIAGNOSTIC_SESSION_CONTROL);
                return;
            }

            uint16_t p2 = (resp->buf[2] << 8) + resp->buf[3];
            uint16_t p2_star = (resp->buf[4] << 8) + resp->buf[5];

            if (p2 >= client->p2_ms) {
                ISO14229USERDEBUG("warning: server P2 timing greater than or equal to client P2 "
                                  "timing (%u >= %u). This may result in timeouts.\n",
                                  p2, client->p2_ms);
            }
            if (p2_star * 10 >= client->p2_star_ms) {
                ISO14229USERDEBUG("warning: server P2* timing greater than or equal to client P2* "
                                  "timing (%u >= %u). This may result in timeouts.\n",
                                  p2_star * 10, client->p2_star_ms);
            }
            break;
        }
        default:
            break;
        }
    }
}

struct SMResult {
    enum Iso14229ClientRequestState state;
    enum Iso14229ClientRequestError err;
};

static struct SMResult _ClientGetNextRequestState(const Iso14229Client *client) {
    struct SMResult result = {.state = client->state, .err = client->err};

    switch (client->state) {
    case kRequestStateIdle: {
        if (ISOTP_RECEIVE_STATUS_FULL == client->link->receive_status) {
            result.err = kRequestErrorUnsolicitedResponse;
        }
        break;
    }

    case kRequestStateSending: {
        switch (client->link->send_status) {
        case ISOTP_SEND_STATUS_INPROGRESS:
            // 等待ISO-TP传输完成
            break;
        case ISOTP_SEND_STATUS_IDLE:
            if (client->suppressPositiveResponse) {
                result.state = kRequestStateIdle;
            } else {
                result.state = kRequestStateSent;
            }
            break;
        case ISOTP_SEND_STATUS_ERROR:
            result.err = kRequestNotSentTransportError;
            break;
        default:
            assert(0);
        }
        break;
    }

    case kRequestStateSent: {
        result.state = kRequestStateSentAwaitResponse;
        break;
    }

    case kRequestStateSentAwaitResponse:
        switch (client->link->receive_status) {
        case ISOTP_RECEIVE_STATUS_FULL:
            result.state = kRequestStateProcessResponse;
            break;
        case ISOTP_RECEIVE_STATUS_IDLE:
        case ISOTP_RECEIVE_STATUS_INPROGRESS:
            if (Iso14229TimeAfter(client->userGetms(), client->p2_timer)) {
                result.state = kRequestStateIdle;
                result.err = kRequestTimedOut;
                printf("timed out. receive status: %d\n", client->link->receive_status);
            }
            break;
        default:
            assert(0);
        }
        break;
    case kRequestStateProcessResponse:
        result.state = kRequestStateIdle;
        break;
    default:
        assert(0);
    }
    return result;
}

static void _ClientProcessRequestState(Iso14229Client *client) {
    IsoTpLink *link = client->link;

    switch (client->state) {
    case kRequestStateIdle: {
        break;
    }
    case kRequestStateSending: {
        break;
    }
    case kRequestStateSent:
        client->p2_timer = client->userGetms() + client->p2_ms;
        break;

    case kRequestStateSentAwaitResponse:
        break;
    case kRequestStateProcessResponse: {
        if (ISOTP_RET_OK == isotp_receive(link, link->receive_buffer, link->receive_buf_size,
                                          &link->receive_size)) {
            client->resp.len = link->receive_size;
            client->state = kRequestStateIdle;
            client->err = _ClientValidateResponse(client);
            if (kRequestNoError == client->err) {
                _ClientHandleResponse(client);
            }
        }
        break;
    }

    default:
        assert(0);
    }
}

static void _ProcessCANRx(Iso14229Client *client) {
    uint32_t arb_id = 0;
    uint8_t data[8] = {0}, size = 0;
    while (kCANRxSome == client->userCANRxPoll(&arb_id, data, &size)) {
        if (arb_id == client->recv_id) {
            isotp_on_can_message(client->link, data, size);
        }
    }
    isotp_poll(client->link);
}

void Iso14229ClientPoll(Iso14229Client *client) {
    struct SMResult result;
    _ProcessCANRx(client);
    result = _ClientGetNextRequestState(client);
    client->state = result.state;
    client->err = result.err;
    if (result.err != kRequestNoError) {
        return;
    }
    _ClientProcessRequestState(client);
}

int iso14229ClientSequenceRunBlocking(Iso14229Client *client, struct Iso14229ClientStep sequence[],
                                      size_t seqLen, int *idx) {
    assert(client);
    assert(client->userGetms);
    assert(client->userCANRxPoll);
    assert(sequence);
    assert(idx);
    assert(seqLen > 0);
    assert(*idx < seqLen);

    while (true) {
        Iso14229ClientPoll(client);
        if (kRequestNoError != client->err) {
            return -1;
        }

        struct Iso14229ClientStep *step = &sequence[*idx];

        switch (step->cb(client, step->args)) {
        case kISO14229_CLIENT_CALLBACK_PENDING:
            break;
        case kISO14229_CLIENT_CALLBACK_DONE: {
            *idx += 1;
            if (*idx >= seqLen) {
                return 0;
            }
            break;
        }
        case kISO14229_CLIENT_CALLBACK_ERROR:
        default:
            return -1;
        }
    }
}
