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

char *Iso14229ClientTaskDescription(Iso14229ClientTask *task) {
#define BUFSIZE 256
    static char buf[BUFSIZE];
    int offset = 0;
    switch (task->taskType) {
    case kTaskTypeDelay:
        offset += snprintf(buf, BUFSIZE - offset, "Delay %dms", task->type.delay.ms);
        break;
    case kTaskTypeServiceCall:
        switch (task->type.serviceCall.sid) {
        case kSID_DIAGNOSTIC_SESSION_CONTROL:
            return "0x10 DIAGNOSTIC SESSION CONTROL";
        case kSID_ECU_RESET:
            return "0x11 ECU RESET";
            offset += snprintf(buf, BUFSIZE - offset, "0x11 EcuReset ");
        case kSID_TESTER_PRESENT:
            return "0x3E TESTER PRESENT";
        default:
            return "unknown";
        }
        break;
    default:
        break;
    }
    return buf;
}

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
    if (kRequestStateSentAwaitResponse == client->ctx.state) {                                     \
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
                                                         enum Iso14229DiagnosticMode mode) {
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

enum Iso14229ClientRequestError
iso14229ClientValidateResponse(const Iso14229ClientRequestContext *ctx) {
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
        return kRequestErrorNegativeResponse;
    }

    return kRequestNoError;
}

void Iso14229ClientPoll(Iso14229Client *client) {
    int ret = 0;
    IsoTpLink *link = client->cfg->link;

    if (-8 != link->send_protocol_result) {
        // printf("send: %d,%d,%d\n", client->ctx.state, link->send_status,
        // link->send_protocol_result); printf("recv: %d,%d,%d\n", client->ctx.state,
        // link->receive_status, link->receive_protocol_result);
    }
    if (-2 == link->send_protocol_result) {
        link->send_protocol_result = -8;
        printf("%d, Error\n", client->cfg->userGetms());
    }

    switch (client->ctx.state) {
    case kRequestStateIdle: {
        ret =
            isotp_receive(link, link->receive_buffer, link->receive_buf_size, &link->receive_size);
        client->ctx.resp.len = link->receive_size;
        if (ISOTP_RET_OK == ret) {
            ISO14229USERDEBUG("Response unexpected: ");
            PRINTHEX(link->receive_buffer, link->receive_size);
            client->ctx.err = kRequestErrorUnsolicitedResponse;
        } else if (ISOTP_RET_NO_DATA == ret) {
            ;
        } else {
            printf("unhandled return value: %d\n", ret);
        }
        break;
    }

    case kRequestStateSending: {
        if (ISOTP_SEND_STATUS_INPROGRESS == client->cfg->link->send_status) {
            ; // Wait until ISO-TP transmission is complete
        } else {
            if (client->ctx.settings.suppressPositiveResponse) {
                client->ctx.state = kRequestStateIdle;
            } else {
                client->p2_timer = client->cfg->userGetms() + client->ctx.settings.p2_ms;
                printf("set p2 timer to %d, current time: %d. p2_ms: %d\n", client->p2_timer,
                       client->cfg->userGetms(), client->ctx.settings.p2_ms);
                client->ctx.state = kRequestStateSentAwaitResponse;
            }
        }
        break;
    }
    case kRequestStateSentAwaitResponse:
        ret =
            isotp_receive(link, link->receive_buffer, link->receive_buf_size, &link->receive_size);
        client->ctx.resp.len = link->receive_size;
        if (ISOTP_RET_OK == ret) {
            client->ctx.state = kRequestStateIdle;
            // ISO14229USERDEBUG("received response\n");
            client->ctx.err = iso14229ClientValidateResponse(&client->ctx);
        } else if (ISOTP_RET_NO_DATA == ret) {
            if (Iso14229TimeAfter(client->cfg->userGetms(), client->p2_timer)) {
                printf("current time: %d. p2_ms: %d\n", client->cfg->userGetms(),
                       client->ctx.settings.p2_ms);
                ISO14229USERDEBUG("timed out\n");
                client->ctx.state = kRequestStateIdle;
                client->ctx.err = kRequestTimedOut;
            }
        } else {
            printf("yet another unhandled return value: %d\n", ret);
        }
        break;
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

// void Iso14229ClientPoll(Iso14229Client *client, UserSequenceFunction func, void *args) {
//     assert(client);
//     assert(client->cfg);
//     Iso14229ClientPoll(client);
//     if (client->ctx.err) {
//         return;
//     }
//     if (func(client, args)) {

//     }
// }