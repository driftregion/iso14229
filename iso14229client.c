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
    assert(client->cfg);
    assert(client->cfg->link);

    memset(&client->ctx, 0, sizeof(client->ctx));

    client->ctx =
        (Iso14229ClientRequestContext){.req =
                                           {
                                               .buf = client->cfg->link->send_buffer,
                                               .buffer_size = client->cfg->link->send_buf_size,
                                               .len = 0,
                                           },
                                       .resp =
                                           {
                                               .buf = client->cfg->link->receive_buffer,
                                               .buffer_size = client->cfg->link->receive_buf_size,
                                               .len = 0,
                                           },
                                       .state = kRequestStateIdle,
                                       .settings = {.functional = {.enable = false, .send_id = 0},
                                                    .suppressPositiveResponse = false}};
}

void iso14229ClientInit(Iso14229Client *client, const Iso14229ClientConfig *cfg) {
    assert(client);
    assert(cfg);
    assert(cfg->link);
    assert(cfg->userGetms);

    memset(client, 0, sizeof(Iso14229Client));
    client->cfg = cfg;
    clearRequestContext(client);
    client->settings = DEFAULT_CLIENT_SETTINGS();
}

static inline enum Iso14229ClientRequestError _SendRequest(Iso14229Client *client) {
    client->ctx.settings = client->settings;
    const Iso14229ClientRequestContext *ctx = &client->ctx;

    if (ctx->settings.suppressPositiveResponse) {
        // ISO14229-1:2013 8.2.2 Table 11
        ctx->req.buf[1] |= 0x80;
    }

    if (ctx->settings.functional.enable) {
        if (ISOTP_RET_OK != isotp_send_with_id(client->cfg->link, ctx->settings.functional.send_id,
                                               ctx->req.buf, ctx->req.len)) {
            return kRequestNotSentTransportError;
        }
    } else {
        if (ISOTP_RET_OK != isotp_send(client->cfg->link, ctx->req.buf, ctx->req.len)) {
            return kRequestNotSentTransportError;
        }
    }
    client->ctx.state = kRequestStateSending;
    return kRequestNoError;
}

#define PRE_REQUEST_CHECK()                                                                        \
    if (kRequestStateIdle != client->ctx.state) {                                                  \
        return kRequestNotSentBusy;                                                                \
    }                                                                                              \
    clearRequestContext(client);

enum Iso14229ClientRequestError ECUReset(Iso14229Client *client,
                                         enum Iso14229ECUResetResetType type) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->buf[0] = kSID_ECU_RESET;
    req->buf[1] = type;
    req->len = 2;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError DiagnosticSessionControl(Iso14229Client *client,
                                                         enum Iso14229DiagnosticSessionType mode) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->buf[0] = kSID_DIAGNOSTIC_SESSION_CONTROL;
    req->buf[1] = mode;
    req->len = 2;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError CommunicationControl(Iso14229Client *client,
                                                     enum Iso14229CommunicationControlType ctrl,
                                                     enum Iso14229CommunicationType comm) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->buf[0] = kSID_COMMUNICATION_CONTROL;
    req->buf[1] = ctrl;
    req->buf[2] = comm;
    req->len = 3;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError TesterPresent(Iso14229Client *client) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
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
    struct Iso14229Request *req = &client->ctx.req;
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
    struct Iso14229Request *req = &client->ctx.req;
    req->buf[0] = kSID_WRITE_DATA_BY_IDENTIFIER;
    if (client->cfg->link->send_buf_size <= 3 || size > client->cfg->link->send_buf_size - 3) {
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
    struct Iso14229Request *req = &client->ctx.req;
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
    struct Iso14229Request *req = &client->ctx.req;
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
                                             const uint16_t blockLength, FILE *fd) {
    PRE_REQUEST_CHECK();
    assert(blockLength > 2); // blockLength must include SID and sequenceCounter
    struct Iso14229Request *req = &client->ctx.req;
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
    struct Iso14229Request *req = &client->ctx.req;
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
    struct Iso14229Request *req = &client->ctx.req;
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
    struct Iso14229Request *req = &client->ctx.req;
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
    if (ISO14229_RESPONSE_SID_OF(kSID_SECURITY_ACCESS) != client->ctx.resp.buf[0]) {
        return kRequestErrorResponseSIDMismatch;
    }
    if (client->ctx.resp.len < ISO14229_0X27_RESP_BASE_LEN) {
        return kRequestErrorResponseTooShort;
    }
    resp->securityAccessType = client->ctx.resp.buf[1];
    resp->securitySeedLength = client->ctx.resp.len - ISO14229_0X27_RESP_BASE_LEN;
    resp->securitySeed = resp->securitySeedLength == 0 ? NULL : &client->ctx.resp.buf[2];
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
    if (ISO14229_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL) != client->ctx.resp.buf[0]) {
        return kRequestErrorResponseSIDMismatch;
    }
    if (client->ctx.resp.len < ISO14229_0X31_RESP_MIN_LEN) {
        return kRequestErrorResponseTooShort;
    }
    resp->routineControlType = client->ctx.resp.buf[1];
    resp->routineIdentifier = (client->ctx.resp.buf[2] << 8) + client->ctx.resp.buf[3];
    resp->routineStatusRecordLength = client->ctx.resp.len - ISO14229_0X31_RESP_MIN_LEN;
    resp->routineStatusRecord = resp->routineStatusRecordLength == 0
                                    ? NULL
                                    : &client->ctx.resp.buf[ISO14229_0X31_RESP_MIN_LEN];
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
enum Iso14229ClientRequestError _ClientValidateResponse(const Iso14229ClientRequestContext *ctx) {

    if (0x7F == ctx->resp.buf[0]) {
        if (ctx->req.buf[0] != ctx->resp.buf[1]) {
            ISO14229USERDEBUG("req->buf[0]: %x, ctx->resp.buf[1]: %x", ctx->req.buf[0],
                              ctx->resp.buf[1]);
            return kRequestErrorResponseSIDMismatch;
        }
        if (kRequestCorrectlyReceived_ResponsePending == ctx->resp.buf[2]) {
            return kRequestNoError;
        } else {
            return kRequestErrorNegativeResponse;
        }
    } else {
        if (ISO14229_RESPONSE_SID_OF(ctx->req.buf[0]) != ctx->resp.buf[0]) {
            ISO14229USERDEBUG("req->buf[0] %x, ctx->resp.buf[0]: %x", ctx->req.buf[0],
                              ctx->resp.buf[0]);
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
    const Iso14229ClientRequestContext *ctx = &client->ctx;

    if (0x7F == ctx->resp.buf[0]) {
        if (kRequestCorrectlyReceived_ResponsePending == ctx->resp.buf[2]) {
            ISO14229USERDEBUG("got RCRRP, setting p2 timer\n");
            client->p2_timer = client->cfg->userGetms() + client->ctx.settings.p2_star_ms;
            client->ctx.state = kRequestStateSentAwaitResponse;
        }
    } else {
        uint8_t respSid = ctx->resp.buf[0];
        switch (ISO14229_REQUEST_SID_OF(respSid)) {
        case kSID_DIAGNOSTIC_SESSION_CONTROL: {
            if (ctx->resp.len < ISO14229_0X10_RESP_LEN) {
                ISO14229USERDEBUG("Error: SID %x response too short\n",
                                  kSID_DIAGNOSTIC_SESSION_CONTROL);
                return;
            }

            uint16_t p2 = (ctx->resp.buf[2] << 8) + ctx->resp.buf[3];
            uint16_t p2_star = (ctx->resp.buf[4] << 8) + ctx->resp.buf[5];

            if (p2 >= client->settings.p2_ms) {
                ISO14229USERDEBUG("warning: server P2 timing greater than or equal to client P2 "
                                  "timing (%u >= %u). This may result in timeouts.\n",
                                  p2, client->settings.p2_ms);
            }
            if (p2_star * 10 >= client->settings.p2_star_ms) {
                ISO14229USERDEBUG("warning: server P2* timing greater than or equal to client P2* "
                                  "timing (%u >= %u). This may result in timeouts.\n",
                                  p2_star * 10, client->settings.p2_star_ms);
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
    struct SMResult result = {.state = client->ctx.state, .err = client->ctx.err};

    switch (client->ctx.state) {
    case kRequestStateIdle: {
        if (ISOTP_RECEIVE_STATUS_FULL == client->cfg->link->receive_status) {
            result.err = kRequestErrorUnsolicitedResponse;
        }
        break;
    }

    case kRequestStateSending: {
        switch (client->cfg->link->send_status) {
        case ISOTP_SEND_STATUS_INPROGRESS:
            // 等待ISO-TP传输完成
            break;
        case ISOTP_SEND_STATUS_IDLE:
            if (client->ctx.settings.suppressPositiveResponse) {
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
        switch (client->cfg->link->receive_status) {
        case ISOTP_RECEIVE_STATUS_FULL:
            result.state = kRequestStateProcessResponse;
            break;
        case ISOTP_RECEIVE_STATUS_IDLE:
        case ISOTP_RECEIVE_STATUS_INPROGRESS:
            if (Iso14229TimeAfter(client->cfg->userGetms(), client->p2_timer)) {
                result.state = kRequestStateIdle;
                result.err = kRequestTimedOut;
                printf("timed out. receive status: %d\n", client->cfg->link->receive_status);
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

void Iso14229ClientPoll(Iso14229Client *client) {
    int ret = 0;
    IsoTpLink *link = client->cfg->link;
    struct SMResult result = _ClientGetNextRequestState(client);
    client->ctx.state = result.state;
    client->ctx.err = result.err;
    if (result.err != kRequestNoError) {
        return;
    }

    switch (client->ctx.state) {
    case kRequestStateIdle: {
        break;
    }
    case kRequestStateSending: {
        break;
    }
    case kRequestStateSent:
        client->p2_timer = client->cfg->userGetms() + client->ctx.settings.p2_ms;
        break;

    case kRequestStateSentAwaitResponse:
        break;
    case kRequestStateProcessResponse: {
        ret =
            isotp_receive(link, link->receive_buffer, link->receive_buf_size, &link->receive_size);
        if (ISOTP_RET_OK == ret) {
            client->ctx.resp.len = link->receive_size;
            client->ctx.state = kRequestStateIdle;
            client->ctx.err = _ClientValidateResponse(&client->ctx);
            if (kRequestNoError == client->ctx.err) {
                _ClientHandleResponse(client);
            }
        }
        break;
    }

    default:
        assert(0);
    }
}

void Iso14229ClientReceiveCAN(Iso14229Client *client, const uint32_t arbitration_id,
                              const uint8_t *data, const uint8_t size) {
    if (arbitration_id == client->cfg->recv_id) {
        isotp_on_can_message(client->cfg->link, (uint8_t *)data, size);
    }
}
