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
                                               .as.raw = client->cfg->link->send_buffer,
                                               .buffer_size = client->cfg->link->send_buf_size,
                                               .len = 0,
                                           },
                                       .resp =
                                           {
                                               .as.raw = client->cfg->link->receive_buffer,
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
        ctx->req.as.base->subFunction |= 0x80;
    }

    if (ctx->settings.functional.enable) {
        if (ISOTP_RET_OK != isotp_send_with_id(client->cfg->link, ctx->settings.functional.send_id,
                                               ctx->req.as.raw, ctx->req.len)) {
            return kRequestNotSentTransportError;
        }
    } else {
        if (ISOTP_RET_OK != isotp_send(client->cfg->link, ctx->req.as.raw, ctx->req.len)) {
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
    req->as.service->sid = kSID_ECU_RESET;
    req->as.service->type.ecuReset.resetType = type;
    req->len = sizeof(ECUResetRequest) + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError DiagnosticSessionControl(Iso14229Client *client,
                                                         enum Iso14229DiagnosticSessionType mode) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_DIAGNOSTIC_SESSION_CONTROL;
    req->as.service->type.diagnosticSessionControl.diagSessionType = mode;
    req->len = sizeof(DiagnosticSessionControlRequest) + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError CommunicationControl(Iso14229Client *client,
                                                     enum Iso14229CommunicationControlType ctrl,
                                                     enum Iso14229CommunicationType comm) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_COMMUNICATION_CONTROL;
    req->as.service->type.communicationControl.controlType = ctrl;
    req->as.service->type.communicationControl.communicationType = comm;
    req->len = sizeof(CommunicationControlRequest) + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError TesterPresent(Iso14229Client *client) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_TESTER_PRESENT;
    req->as.service->type.testerPresent.zeroSubFunction = 0;
    req->len = sizeof(TesterPresentRequest) + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError ReadDataByIdentifier(Iso14229Client *client,
                                                     const uint16_t *didList,
                                                     const uint16_t numDataIdentifiers) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_READ_DATA_BY_IDENTIFIER;
    for (int i = 0; i < numDataIdentifiers; i++) {
        uint16_t offset = 1 + sizeof(uint16_t) * i;
        if (offset + 2 > req->buffer_size) {
            assert(0);
            return kRequestNotSentInvalidArgs;
        }
        *(uint16_t *)(req->as.raw + offset) = Iso14229htons(didList[i]);
    }
    req->len = (numDataIdentifiers * sizeof(uint16_t)) + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError WriteDataByIdentifier(Iso14229Client *client,
                                                      uint16_t dataIdentifier, const uint8_t *data,
                                                      uint16_t size) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_WRITE_DATA_BY_IDENTIFIER;
    if (size >
        client->cfg->link->send_buf_size - offsetof(WriteDataByIdentifierRequest, dataRecord)) {
        return kRequestNotSentBufferTooSmall;
    }

    req->as.service->type.writeDataByIdentifier.dataIdentifier = Iso14229htons(dataIdentifier);
    memmove(req->as.service->type.writeDataByIdentifier.dataRecord, data, size);

    req->len = sizeof(WriteDataByIdentifierRequest) + size + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError RoutineControl(Iso14229Client *client, enum RoutineControlType type,
                                               uint16_t routineIdentifier, uint8_t *data,
                                               uint16_t size) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_ROUTINE_CONTROL;
    req->as.service->type.routineControl.routineControlType = type;
    req->as.service->type.routineControl.routineIdentifier = Iso14229htons(routineIdentifier);
    if (size) {
        assert(data);
        if (size > req->buffer_size - offsetof(RoutineControlRequest, routineControlOptionRecord)) {
            return kRequestNotSentBufferTooSmall;
        }
        memmove(req->as.service->type.routineControl.routineControlOptionRecord, data, size);
    }
    req->len = offsetof(RoutineControlRequest, routineControlOptionRecord) + size + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError RequestDownload(Iso14229Client *client,
                                                uint8_t dataFormatIdentifier,
                                                uint8_t addressAndLengthFormatIdentifier,
                                                size_t memoryAddress, size_t memorySize) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    uint8_t numMemorySizeBytes = (addressAndLengthFormatIdentifier & 0xF0) >> 4;
    uint8_t numMemoryAddressBytes = addressAndLengthFormatIdentifier & 0x0F;

    req->as.service->sid = kSID_REQUEST_DOWNLOAD;
    req->as.service->type.requestDownload.dataFormatIdentifier = dataFormatIdentifier;
    req->as.service->type.requestDownload.addressAndLengthFormatIdentifier =
        addressAndLengthFormatIdentifier;

    uint8_t *ptr =
        ((uint8_t *)&req->as.service->type.requestDownload.addressAndLengthFormatIdentifier) + 1;
    uint8_t *var_len_start = ptr;

    while (numMemoryAddressBytes--) {
        *ptr =
            (memoryAddress & (0xFF << (8 * numMemoryAddressBytes))) >> (8 * numMemoryAddressBytes);
        ptr++;
    }

    while (numMemorySizeBytes--) {
        *ptr = (memorySize & (0xFF << (8 * numMemorySizeBytes))) >> (8 * numMemorySizeBytes);
        ptr++;
    }

    req->len = ptr - var_len_start + offsetof(RequestDownloadRequest, memoryAddress) + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError RequestDownload_32_32(Iso14229Client *client,
                                                      uint32_t memoryAddress, uint32_t memorySize) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_REQUEST_DOWNLOAD;
    req->as.service->type.requestDownload.dataFormatIdentifier = 0;
    req->as.service->type.requestDownload.addressAndLengthFormatIdentifier = 0x44;
    req->as.service->type.requestDownload.memoryAddress = Iso14229htonl(memoryAddress);
    req->as.service->type.requestDownload.memorySize = Iso14229htonl(memorySize);
    req->len = sizeof(RequestDownloadRequest) + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError TransferData(Iso14229Client *client, uint8_t blockSequenceCounter,
                                             const uint16_t blockLength, FILE *fd) {
    PRE_REQUEST_CHECK();
    assert(blockLength > 2); // blockLength must include SID and sequenceCounter
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_TRANSFER_DATA;
    req->as.service->type.transferData.blockSequenceCounter = blockSequenceCounter;
    uint16_t size = fread(req->as.service->type.transferData.data, 1, blockLength - 2, fd);
    ISO14229USERDEBUG("size: %d, blocklength: %d\n", size, blockLength);
    req->len = sizeof(TransferDataRequest) + size + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError RequestTransferExit(Iso14229Client *client) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    req->as.service->sid = kSID_REQUEST_TRANSFER_EXIT;
    req->len = 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError ControlDTCSetting(Iso14229Client *client, uint8_t dtcSettingType,
                                                  uint8_t *data, uint16_t size) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    if (0x00 == dtcSettingType || 0x7F == dtcSettingType ||
        (0x03 <= dtcSettingType && dtcSettingType <= 0x3F)) {
        assert(0); // reserved vals
    }

    if (NULL == data) {
        assert(size == 0);
    } else {
        assert(size > 0);
        if (size >
            client->cfg->link->send_buf_size - offsetof(ControlDtcSettingRequest, dtcSettingType)) {
            return kRequestNotSentBufferTooSmall;
        }
        memmove(req->as.service->type.controlDtcSetting.dtcSettingControlOptionRecord, data, size);
    }

    req->as.base->sid = kSID_CONTROL_DTC_SETTING;
    req->as.service->type.controlDtcSetting.dtcSettingType = dtcSettingType;
    req->len = sizeof(ControlDtcSettingRequest) + size + 1;
    return _SendRequest(client);
}

enum Iso14229ClientRequestError SecurityAccess(Iso14229Client *client, uint8_t level, uint8_t *data,
                                               uint16_t size) {
    PRE_REQUEST_CHECK();
    struct Iso14229Request *req = &client->ctx.req;
    if (Iso14229SecurityAccessLevelIsReserved(level)) {
        return kRequestNotSentInvalidArgs;
    }
    req->as.base->sid = kSID_SECURITY_ACCESS;
    req->as.base->subFunction = level;
    if (size > client->cfg->link->send_buf_size + offsetof(struct Iso14229GenericRequest, data)) {
        return kRequestNotSentBufferTooSmall;
    }
    memmove(req->as.base->data, data, size);
    req->len = sizeof(SecurityAccessRequest) + size + 1;
    return _SendRequest(client);
}

/**
 * @brief Check that the response is standard
 *
 * @param ctx
 * @return enum Iso14229ClientRequestError
 */
enum Iso14229ClientRequestError _ClientValidateResponse(const Iso14229ClientRequestContext *ctx) {
    uint8_t responseSid;

    if (responseIsNegative(&ctx->resp)) {
        responseSid = ctx->resp.as.negative->requestSid;
    } else {
        responseSid = ctx->resp.as.positive->serviceId;
        if (ctx->req.as.service->sid != ISO14229_REQUEST_SID_OF(responseSid)) {
            return kRequestErrorResponseSIDMismatch;
        }
    }

    if (responseIsNegative(&ctx->resp)) {
        if (kRequestCorrectlyReceived_ResponsePending == ctx->resp.as.negative->responseCode) {
            return kRequestNoError;
        }
        return kRequestErrorNegativeResponse;
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

    if (responseIsNegative(&ctx->resp)) {
        if (kRequestCorrectlyReceived_ResponsePending == ctx->resp.as.negative->responseCode) {
            printf("got RCRRP, setting p2 timer\n");
            client->p2_timer = client->cfg->userGetms() + client->ctx.settings.p2_star_ms;
            client->ctx.state = kRequestStateSentAwaitResponse;
        }
    } else {
        switch (ISO14229_REQUEST_SID_OF(ctx->resp.as.positive->serviceId)) {
        case kSID_DIAGNOSTIC_SESSION_CONTROL: {
            const DiagnosticSessionControlResponse *resp =
                &ctx->resp.as.positive->type.diagnosticSessionControl;
            if (Iso14229ntohs(resp->P2) >= client->settings.p2_ms) {
                ISO14229USERDEBUG("warning: server P2 timing greater than or equal to client P2 "
                                  "timing (%u >= %u). This may result in timeouts.\n",
                                  Iso14229ntohs(resp->P2), client->settings.p2_ms);
            }
            if (Iso14229ntohs(resp->P2star) * 10 >= client->settings.p2_star_ms) {
                ISO14229USERDEBUG("warning: server P2* timing greater than or equal to client P2* "
                                  "timing (%u >= %u). This may result in timeouts.\n",
                                  Iso14229ntohs(resp->P2star) * 10, client->settings.p2_star_ms);
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
            ISO14229USERDEBUG("Response unexpected");
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
