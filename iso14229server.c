/**
 * @file iso14229server.c
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "iso14229.h"
#include "iso14229server.h"
#include "isotp-c/isotp.h"
#include "isotp-c/isotp_defines.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

static inline void iso14229DownloadHandlerInit(Iso14229DownloadHandler *handler);

// ========================================================================
//                              Private Functions
// ========================================================================

/**
 * @brief Convenience function to send negative response
 *
 * @param self
 * @param response_code
 */
static inline enum Iso14229ResponseCode
iso14229SendNegativeResponse(Iso14229ServerRequestContext *ctx,
                             enum Iso14229ResponseCode response_code) {
    ctx->resp.as.negative->negResponseSid = 0x7F;
    ctx->resp.as.negative->requestSid = ctx->req.sid;
    ctx->resp.as.negative->responseCode = response_code;
    ctx->resp.len = sizeof(struct Iso14229NegativeResponse);
    return response_code;
}

static inline enum Iso14229ResponseCode
iso14229SendPositiveResponse(Iso14229ServerRequestContext *ctx, const uint16_t len) {
    ctx->resp.as.positive->serviceId = ISO14229_RESPONSE_SID_OF(ctx->req.sid);
    ctx->resp.len = offsetof(struct Iso14229PositiveResponse, type) + len;
    return kPositiveResponse;
}

static inline void iso14229SendNoResponse(Iso14229ServerRequestContext *ctx) { ctx->resp.len = 0; }

/**
 * @brief 0x10 DiagnosticSessionControl
 *
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229DiagnosticSessionControl(Iso14229Server *self,
                                                           Iso14229ServerRequestContext *ctx) {
    DiagnosticSessionControlResponse *response =
        &ctx->resp.as.positive->type.diagnosticSessionControl;

    uint8_t diagSessionType = 0;
    if (ctx->req.len < 1) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    diagSessionType = ctx->req.as.raw[0] & 0x4F;

    // TODO: add user-defined diag modes
    switch (diagSessionType) {
    case kDiagModeDefault:
    case kDiagModeProgramming:
    case kDiagModeExtendedDiagnostic:
        break;
    default:
        return iso14229SendNegativeResponse(ctx, kSubFunctionNotSupported);
    }

    self->diag_mode = diagSessionType;

    response->diagSessionType = diagSessionType;

    // ISO14229-1-2013: Table 29
    // resolution: 1ms
    response->P2 = Iso14229htons(self->cfg->p2_ms);
    // resolution: 10ms
    response->P2star = Iso14229htons(self->cfg->p2_star_ms / 10);

    return iso14229SendPositiveResponse(ctx, sizeof(DiagnosticSessionControlResponse));
}

static inline void scheduleECUReset(Iso14229Server *self) {
    assert(self->cfg->userHardReset);
    self->notReadyToReceive = true;
    self->ecu_reset_100ms_timer = self->cfg->userGetms() + 100;
    self->ecuResetScheduled = true;
}

/**
 * @brief 0x11 ECUReset
 *
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229ECUReset(Iso14229Server *self,
                                           Iso14229ServerRequestContext *ctx) {
    ECUResetResponse *response = &ctx->resp.as.positive->type.ecuReset;
    uint8_t resetType = 0;
    if (ctx->req.len < 1) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    resetType = ctx->req.as.raw[0] & 0x3F;

    if (kHardReset == resetType) {
        if (!self->ecuResetScheduled) {
            scheduleECUReset(self);
        }
    }

    response->resetType = resetType;
    response->powerDownTime = 0;

    return iso14229SendPositiveResponse(ctx, sizeof(ECUResetResponse));
}

/**
 * @brief 0x22 ReadDataByIdentifier
 * @addtogroup readDataByIdentifier_0x22
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229ReadDataByIdentifier(Iso14229Server *self,
                                                       Iso14229ServerRequestContext *ctx) {
    ReadDataByIdentifierResponse *response = &ctx->resp.as.positive->type.readDataByIdentifier;
    uint8_t numDIDs = ctx->req.len / sizeof(uint16_t);
    const uint8_t *data_location = NULL;
    uint16_t dataRecordSize = 0;
    uint16_t responseLength = 0;
    uint16_t dataId = 0;
    enum Iso14229ResponseCode rdbi_response;

    if (NULL == self->cfg->userRDBIHandler) {
        return iso14229SendNegativeResponse(ctx, kServiceNotSupported);
    }

    if (ctx->req.len % sizeof(uint16_t) != 0) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    if (0 == numDIDs) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (int i = 0; i < numDIDs; i++) {
        // TODO: make this safe
        dataId = Iso14229ntohs(*((uint16_t *)ctx->req.as.raw + i));

        rdbi_response = self->cfg->userRDBIHandler(dataId, &data_location, &dataRecordSize);
        if (kPositiveResponse == rdbi_response) {
            // TODO: make this safe: ensure that the offset doesn't exceed the
            // response buffer
            uint8_t *offset = ((uint8_t *)response) + responseLength;

            *(uint16_t *)(offset) = Iso14229htons(dataId);
            memmove(offset + sizeof(uint16_t), data_location, dataRecordSize);

            responseLength += sizeof(uint16_t) + dataRecordSize;
        } else {
            return iso14229SendNegativeResponse(ctx, rdbi_response);
        }
    }

    return iso14229SendPositiveResponse(ctx, sizeof(ReadDataByIdentifierResponse) + responseLength);
}

enum Iso14229ResponseCode iso14299SecurityAccess(Iso14229Server *self,
                                                 Iso14229ServerRequestContext *ctx) {
    uint8_t subFunction = ctx->req.as.securityAccess->subFunction;

    if (0x00 == subFunction ||                          // ISOSAEReserved
        (subFunction >= 0x43 && subFunction <= 0x5E) || // ISOSAEReserved
        0x7F == subFunction)                            // ISOSAEReserved
    {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    // Even: sendKey
    if (0 == subFunction % 2) {
        return iso14229SendNegativeResponse(ctx, kGeneralProgrammingFailure);
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
        if (self->securityLevel == subFunction) {
            ctx->resp.as.positive->type.securityAccess.securitySeed[0] = 0;
            return iso14229SendPositiveResponse(ctx, sizeof(SecurityAccessResponse) + 1);
        }
    }
    return iso14229SendNegativeResponse(ctx, kGeneralProgrammingFailure);
}

/**
 * @brief 0x28 CommunicationControl
 *
 * @addtogroup communicationControl_0x28
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229CommunicationControl(Iso14229Server *self,
                                                       Iso14229ServerRequestContext *ctx) {
    const CommunicationControlRequest *request = ctx->req.as.communicationControl;
    CommunicationControlResponse *response = &ctx->resp.as.positive->type.communicationControl;
    enum Iso14229ResponseCode callback_response = kPositiveResponse;

    // The message is not fixed-length. existence of nodeIdentificationNumber depends on controlType
    if (ctx->req.len < offsetof(CommunicationControlRequest, communicationType)) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    if (NULL == self->cfg->userCommunicationControlHandler) {
        return iso14229SendNegativeResponse(ctx, kServiceNotSupported);
    }

    callback_response = self->cfg->userCommunicationControlHandler(request->controlType,
                                                                   request->communicationType);
    if (kPositiveResponse != callback_response) {
        return iso14229SendNegativeResponse(ctx, callback_response);
    }
    response->controlType = request->controlType;
    return iso14229SendPositiveResponse(ctx, sizeof(CommunicationControlResponse));
}

/**
 * @brief 0x2E WriteDataByIdentifier
 *
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229WriteDataByIdentifier(Iso14229Server *self,
                                                        Iso14229ServerRequestContext *ctx) {
    WriteDataByIdentifierResponse *response = &ctx->resp.as.positive->type.writeDataByIdentifier;
    const WriteDataByIdentifierRequest *request = ctx->req.as.writeDataByIdentifier;

    uint16_t dataLen = 0;
    uint16_t dataId = 0;
    enum Iso14229ResponseCode wdbi_response;

    /* Must contain at least one byte */
    if (ctx->req.len < sizeof(uint16_t) + 1) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    dataId = Iso14229ntohs(request->dataIdentifier);
    dataLen = ctx->req.len - offsetof(WriteDataByIdentifierRequest, dataRecord);

    response->dataId = Iso14229htons(dataId);

    if (NULL != self->cfg->userWDBIHandler) {
        wdbi_response = self->cfg->userWDBIHandler(dataId, request->dataRecord, dataLen);
        if (kPositiveResponse != wdbi_response) {
            return iso14229SendNegativeResponse(ctx, wdbi_response);
        }
    } else {
        return iso14229SendNegativeResponse(ctx, kServiceNotSupported);
    }

    return iso14229SendPositiveResponse(ctx, sizeof(WriteDataByIdentifierResponse));
}

/**
 * @brief 0x31 RoutineControl
 *
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229RoutineControl(Iso14229Server *self,
                                                 Iso14229ServerRequestContext *ctx) {
    RoutineControlResponse *response = &ctx->resp.as.positive->type.routineControl;
    const RoutineControlRequest *request = ctx->req.as.routineControl;
    enum Iso14229ResponseCode responseCode = kPositiveResponse;
    uint16_t routineIdentifier = Iso14229ntohs(request->routineIdentifier);

    if (ctx->req.len < sizeof(RoutineControlRequest)) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    const Iso14229Routine *routine = NULL;
    for (uint16_t i = 0; i < self->nRegisteredRoutines; i++) {
        if (self->routines[i]->routineIdentifier == routineIdentifier) {
            routine = self->routines[i];
        }
    }

    // The subfunction corresponding to this routineIdentifier
    if (routine == NULL) {
        return iso14229SendNegativeResponse(ctx, kSubFunctionNotSupported);
    }

    // The actual statusRecord length written by the routine
    uint16_t statusRecordLength = 0;

    Iso14229RoutineControlArgs args = {
        .optionRecord = request->routineControlOptionRecord,
        .optionRecordLength = offsetof(RoutineControlRequest, routineControlOptionRecord),
        .statusRecord = response->routineStatusRecord,
        // User response limited to the size of the ISO-TP send buffer minus the size of the
        // spec-mandated header
        .statusRecordBufferSize =
            ctx->resp.buffer_size - offsetof(RoutineControlResponse, routineStatusRecord),
        .statusRecordLength = &statusRecordLength,
    };

    switch (request->routineControlType) {
    case kStartRoutine:
        if (NULL != routine->startRoutine) {
            responseCode = routine->startRoutine(routine->userCtx, &args);
        } else {
            return iso14229SendNegativeResponse(ctx, kSubFunctionNotSupported);
        }
        break;
    case kStopRoutine:
        if (NULL != routine->stopRoutine) {
            responseCode = routine->stopRoutine(routine->userCtx, &args);
        } else {
            return iso14229SendNegativeResponse(ctx, kSubFunctionNotSupported);
        }
        break;
    case kRequestRoutineResults:
        if (NULL != routine->requestRoutineResults) {
            responseCode = routine->requestRoutineResults(routine->userCtx, &args);
        } else {
            return iso14229SendNegativeResponse(ctx, kSubFunctionNotSupported);
        }
        break;
    default:
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    if (statusRecordLength > args.statusRecordBufferSize) {
        responseCode = kGeneralProgrammingFailure;
    }

    if (kPositiveResponse != responseCode) {
        return iso14229SendNegativeResponse(ctx, responseCode);
    }

    response->routineControlType = request->routineControlType;
    response->routineIdentifier = Iso14229htons(routineIdentifier);

    return iso14229SendPositiveResponse(ctx, sizeof(RoutineControlResponse) + statusRecordLength);
}

/**
 * @brief 0x34 RequestDownload
 *
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229RequestDownload(Iso14229Server *self,
                                                  Iso14229ServerRequestContext *ctx) {
    RequestDownloadResponse *response = &ctx->resp.as.positive->type.requestDownload;
    const RequestDownloadRequest *request = ctx->req.as.requestDownload;

    const Iso14229DownloadHandler *handler = NULL;
    enum Iso14229ResponseCode err;
    uint16_t maxNumberOfBlockLength = 0;
    void *memoryAddress = NULL;
    uint32_t memorySize = 0;

    if (ctx->req.len < sizeof(RequestDownloadRequest)) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t memorySizeLength = (request->addressAndLengthFormatIdentifier & 0xF0) >> 4;
    uint8_t memoryAddressLength = request->addressAndLengthFormatIdentifier & 0x0F;

    // ASSUMPTION: This server implementation only supports 32 bit memory
    // addressing
    if (memorySizeLength != sizeof(uint32_t) || memoryAddressLength != sizeof(uint32_t)) {
        return iso14229SendNegativeResponse(ctx, kRequestOutOfRange);
    }

    memoryAddress = (void *)((size_t)Iso14229ntohl(request->memoryAddress));
    memorySize = Iso14229ntohl(request->memorySize);

    // TODO: not yet implemented multiple Upload/Download handlers
    // This will need some documented heuristic for determining the correct
    // handler, probably a map of {memoryAddress: handler}
    if (self->nRegisteredDownloadHandlers < 1) {
        return iso14229SendNegativeResponse(ctx, kUploadDownloadNotAccepted);
    }
    handler = self->downloadHandlers[0];

    err = handler->cfg->onRequest(handler->cfg->userCtx, request->dataFormatIdentifier,
                                  memoryAddress, memorySize, &maxNumberOfBlockLength);

    if (err != kPositiveResponse) {
        return iso14229SendNegativeResponse(ctx, err);
    }

    if (0 == maxNumberOfBlockLength) {
        ISO14229USERDEBUG("WARNING: maxNumberOfBlockLength not set");
        return iso14229SendNegativeResponse(ctx, kGeneralProgrammingFailure);
    }

    // ISO-14229-1:2013 Table 401:
    // ASSUMPTION: use fixed size of maxNumberOfBlockLength in RequestDownload
    // response: 2 bytes
    response->lengthFormatIdentifier = 0x20;

// ISO-15764-2-2004 section 5.3.3
#define ISOTP_MTU 4095UL

/* ISO-14229-1:2013 Table 396: maxNumberOfBlockLength
This parameter is used by the requestDownload positive response message to
inform the client how many data bytes (maxNumberOfBlockLength) to include in
each TransferData request message from the client. This length reflects the
complete message length, including the service identifier and the
data-parameters present in the TransferData request message.
*/
#define MAX_TRANSFER_DATA_PAYLOAD_LEN (ISOTP_MTU)

    response->maxNumberOfBlockLength =
        Iso14229htons(MIN(maxNumberOfBlockLength, MAX_TRANSFER_DATA_PAYLOAD_LEN));
    return iso14229SendPositiveResponse(ctx, sizeof(RequestDownloadResponse));
}

/**
 * @addtogroup transferData_0x36
 */
static inline int blockSequenceNumberIsBad(const uint8_t block_counter,
                                           const Iso14229DownloadHandler *handler) {
    if (block_counter != handler->blockSequenceCounter) {
        return 1;
    }
    return 0;
}

/**
 * @brief 0x36 TransferData
 * @addtogroup transferData_0x36
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229TransferData(Iso14229Server *self,
                                               Iso14229ServerRequestContext *ctx) {
    TransferDataResponse *response = &ctx->resp.as.positive->type.transferData;
    const TransferDataRequest *request = ctx->req.as.transferData;
    Iso14229DownloadHandler *handler = NULL;
    enum Iso14229ResponseCode err;

    uint16_t request_data_len = ctx->req.len - offsetof(TransferDataRequest, data);

    if (ctx->req.len < sizeof(TransferDataRequest)) {
        err = kIncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    if (self->nRegisteredDownloadHandlers < 1) {
        err = kUploadDownloadNotAccepted;
        goto fail;
    }

    handler = self->downloadHandlers[0];

    if (blockSequenceNumberIsBad(request->blockSequenceCounter, handler)) {
        err = kRequestSequenceError;
        goto fail;
    } else {
        handler->blockSequenceCounter++;
    }

    err = handler->cfg->onTransfer(handler->cfg->userCtx, request->data, request_data_len);
    if (err != kPositiveResponse) {
        goto fail;
    }

    response->blockSequenceCounter = request->blockSequenceCounter;

    return iso14229SendPositiveResponse(ctx, sizeof(TransferDataResponse));

// There's been an error. Reinitialize the handler to clear out its state
fail:
    iso14229DownloadHandlerInit(handler);
    return iso14229SendNegativeResponse(ctx, err);
}

/**
 * @brief 0x37 RequestTransferExit
 * @addtogroup requestTransferExit_0x37
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229RequestTransferExit(Iso14229Server *self,
                                                      Iso14229ServerRequestContext *ctx) {
    Iso14229DownloadHandler *handler = NULL;
    enum Iso14229ResponseCode err;

    if (self->nRegisteredDownloadHandlers < 1) {
        return iso14229SendNegativeResponse(ctx, kUploadDownloadNotAccepted);
    }
    handler = self->downloadHandlers[0];

    err = handler->cfg->onExit(handler->cfg->userCtx);

    if (err != kPositiveResponse) {
        return iso14229SendNegativeResponse(ctx, err);
    }

    iso14229DownloadHandlerInit(handler);

    return iso14229SendPositiveResponse(ctx, sizeof(RequestTransferExitResponse));
}

/**
 * @brief 0x3E TesterPresent
 *
 * @param self
 * @param data
 * @param size
 */
enum Iso14229ResponseCode iso14229TesterPresent(Iso14229Server *self,
                                                Iso14229ServerRequestContext *ctx) {
    TesterPresentResponse *response = &ctx->resp.as.positive->type.testerPresent;
    const TesterPresentRequest *request = ctx->req.as.testerPresent;

    if (ctx->req.len < 1) {
        return iso14229SendNegativeResponse(ctx, kIncorrectMessageLengthOrInvalidFormat);
    }

    self->s3_session_timeout_timer = self->cfg->userGetms() + self->cfg->s3_ms;
    response->zeroSubFunction = request->zeroSubFunction & 0x3F;
    iso14229SendPositiveResponse(ctx, sizeof(TesterPresentResponse));
    return kPositiveResponse;
}

/**
 * @brief Call the service if it exists, modifying the response if the spec calls for it.
 * @note see ISO14229-1 2013 7.5.5 Pseudo code example of server response behavior
 *
 * @param self
 * @param service NULL if the service is not supported
 * @param ctx
 */
static enum Iso14229ResponseCode
iso14229EvaluateServiceResponse(Iso14229Server *self, Iso14229Service service,
                                Iso14229ServerRequestContext *ctx) {
    enum Iso14229ResponseCode response = kPositiveResponse;
    bool suppressResponse = false;

    if (NULL == service) {
        return iso14229SendNegativeResponse(ctx, kServiceNotSupported);
    }

    switch (ctx->req.sid) {
    /* CASE Service_with_sub-function */
    /* test if service with sub-function is supported */
    case kSID_DIAGNOSTIC_SESSION_CONTROL:
    case kSID_ECU_RESET:
    case kSID_READ_DTC_INFORMATION:
    case kSID_SECURITY_ACCESS:
    case kSID_COMMUNICATION_CONTROL:
    case kSID_ROUTINE_CONTROL:
    case kSID_TESTER_PRESENT:
    case kSID_ACCESS_TIMING_PARAMETER:
    case kSID_SECURED_DATA_TRANSMISSION:
    case kSID_CONTROL_DTC_SETTING:
    case kSID_RESPONSE_ON_EVENT:

        /* check minimum length of message with sub-function */
        /* NOTE: the pseudo-code uses 2 here, but ctx->req.len already has the SID subtracted */
        if (ctx->req.len >= 1) {
            /* get sub-function parameter value without bit 7 */
            // switch (ctx->req.as.raw[0] & 0x7F) {

            // }
            // Let the service callback determine whether or not the sub-function parameter value is
            // supported
            response = service(self, ctx);
        } else {
            /* NRC 0x13: incorrectMessageLengthOrInvalidFormat */
            response = kIncorrectMessageLengthOrInvalidFormat;
        }

        bool suppressPosRspMsgIndicationBit = ctx->req.as.raw[0] & 0x80;

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

    /* CASE Service_without_sub-function */
    /* test if service without sub-function is supported */
    case kSID_READ_DATA_BY_IDENTIFIER:
    case kSID_READ_MEMORY_BY_ADDRESS:
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
    case kSID_WRITE_DATA_BY_IDENTIFIER:
    case kSID_REQUEST_DOWNLOAD:
    case kSID_REQUEST_UPLOAD:
    case kSID_TRANSFER_DATA:
    case kSID_REQUEST_TRANSFER_EXIT:
    case kSID_REQUEST_FILE_TRANSFER:
    case kSID_WRITE_MEMORY_BY_ADDRESS:
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
    case kSID_INPUT_CONTROL_BY_IDENTIFIER:
        response = service(self, ctx);
        break;

    default:
        response = kServiceNotSupported;
        break;
    }

    if ((kAddressingSchemeFunctional == ctx->req.addressingScheme) &&
        ((kServiceNotSupported == response) || (kSubFunctionNotSupported == response) ||
         (kServiceNotSupportedInActiveSession == response) ||
         (kSubFunctionNotSupportedInActiveSession == response) ||
         (kRequestOutOfRange == response)) &&
        (
            // TODO: *not yet a NRC 0x78 response sent*
            true)) {
        suppressResponse = true; /* Suppress negative response message */
        iso14229SendNoResponse(ctx);
    } else {
        if (suppressResponse) { /* Suppress positive response message */
            iso14229SendNoResponse(ctx);
        } else { /* send negative or positive response */
            ;
        }
    }
    return response;
}

/**
 * @brief Call the service matching the requested SID
 *
 * @param self
 * @param buf   incoming data from ISO-TP layer
 * @param size  size of buf
 */
void iso14229CallRequestedService(Iso14229Server *self, IsoTpLink *link,
                                  enum Iso14229AddressingScheme addressingScheme) {

    Iso14229ServerRequestContext ctx = {
        .req =
            {
                .as.raw = self->cfg->receive_buffer + 1,
                .len = self->receive_size - 1,
                .sid = self->cfg->receive_buffer[0],
                .addressingScheme = addressingScheme,
            },
        .resp = {.as.raw = self->cfg->send_buffer,
                 .len = 0,
                 .buffer_size = self->cfg->send_buf_size},
    };

    Iso14229Service service = self->services[ctx.req.sid];
    enum Iso14229ResponseCode response = iso14229EvaluateServiceResponse(self, service, &ctx);

    if (kRequestCorrectlyReceived_ResponsePending == response) {
        self->RCRRP = true;
        self->notReadyToReceive = true;
    } else {
        self->RCRRP = false;
    }

    if (ctx.resp.len) {
        isotp_send(link, ctx.resp.as.raw, ctx.resp.len);
    }
}

// ========================================================================
//                             Public Functions
// ========================================================================

/**
 * @brief \~chinese 初始化服务器 \~english Initialize the server
 *
 * @param self
 * @param cfg
 * @return int
 */
void Iso14229ServerInit(Iso14229Server *self, const Iso14229ServerConfig *const cfg) {
    assert(self);
    assert(cfg);
    assert(cfg->userGetms);
    assert(cfg->phys_link);
    assert(cfg->func_link);
    assert(cfg->send_buffer);
    assert(cfg->receive_buffer);
    assert(cfg->send_buf_size > 2);
    assert(cfg->receive_buf_size > 2);

    memset(self, 0, sizeof(Iso14229Server));
    self->cfg = cfg;

    // Initialize p2_timer to an already past time, otherwise the server's
    // response to incoming messages will be delayed.
    self->p2_timer = self->cfg->userGetms() - self->cfg->p2_ms;

    // Set the session timeout for s3 milliseconds from now.
    self->s3_session_timeout_timer = self->cfg->userGetms() + self->cfg->s3_ms;

    if (NULL != cfg->middleware) {
        Iso14229UserMiddleware *mw = cfg->middleware;
        assert(mw->initFunc);
        assert(0 == mw->initFunc(mw->self, mw->cfg, self));
    }
}

/**
 * @brief ISO14229-1-2013 Figure 4
 *
 * @param self
 * @return int
 */
int iso14229StateMachine(Iso14229Server *self) {
    if (self->ecuResetScheduled &&
        (Iso14229TimeAfter(self->cfg->userGetms(), self->ecu_reset_100ms_timer))) {
        assert(self->cfg->userHardReset);
        self->cfg->userHardReset();
    }
    return 0;
}

static inline void iso14229ProcessLink(Iso14229Server *self, IsoTpLink *link,
                                       enum Iso14229AddressingScheme addressingScheme) {

    // If the user service handler responded RCRRP and the send link is now idle,
    // the response has been sent and the long-running service can now be called.
    if (self->RCRRP && ISOTP_SEND_STATUS_IDLE == link->send_status) {
        iso14229CallRequestedService(self, link, addressingScheme);
        self->notReadyToReceive = self->RCRRP;
    } else if (self->notReadyToReceive) {
        return;
    } else if (Iso14229TimeAfter(self->cfg->userGetms(), self->p2_timer)) {
        int recv_status = isotp_receive(link, self->cfg->receive_buffer,
                                        self->cfg->receive_buf_size, &self->receive_size);
        switch (recv_status) {
        case ISOTP_RET_OK:
            iso14229CallRequestedService(self, link, addressingScheme);
            self->p2_timer = self->cfg->userGetms() + self->cfg->p2_ms;
            break;
        case ISOTP_RET_NO_DATA:
            break;
        default:
            assert(0);
            break;
        }
    }
}

void Iso14229ServerPoll(Iso14229Server *self) {
    const Iso14229ServerConfig *cfg = self->cfg;

    isotp_poll(cfg->phys_link);
    isotp_poll(cfg->func_link);

    iso14229StateMachine(self);

    // Run middleware if installed
    if (NULL != cfg->middleware && NULL != cfg->middleware->pollFunc) {
        cfg->middleware->pollFunc(cfg->middleware->self, self);
    }

    iso14229ProcessLink(self, self->cfg->phys_link, kAddressingSchemePhysical);
    iso14229ProcessLink(self, self->cfg->func_link, kAddressingSchemeFunctional);
}

void iso14229ServerReceiveCAN(Iso14229Server *self, const uint32_t arbitration_id,
                              const uint8_t *data, const uint8_t size) {

    if (arbitration_id == self->cfg->phys_recv_id) {
        isotp_on_can_message(self->cfg->phys_link, (uint8_t *)data, size);
    } else if (arbitration_id == self->cfg->func_recv_id) {
        isotp_on_can_message(self->cfg->func_link, (uint8_t *)data, size);
    } else {
        return;
    }
}

int Iso14229ServerRegisterRoutine(Iso14229Server *self, const Iso14229Routine *routine) {
    if ((self->nRegisteredRoutines >= ISO14229_SERVER_MAX_ROUTINES) || (routine == NULL) ||
        (routine->startRoutine == NULL)) {
        return -1;
    }

    self->routines[self->nRegisteredRoutines] = routine;
    self->nRegisteredRoutines++;
    return 0;
}

static inline void iso14229DownloadHandlerInit(Iso14229DownloadHandler *handler) {
    handler->isActive = false;
    handler->blockSequenceCounter = 1;
}

int iso14229ServerRegisterDownloadHandler(Iso14229Server *self, Iso14229DownloadHandler *handler,
                                          Iso14229DownloadHandlerConfig *cfg) {
    if ((self->nRegisteredDownloadHandlers >= ISO14229_SERVER_MAX_DOWNLOAD_HANDLERS) ||
        handler == NULL || cfg->onRequest == NULL || cfg->onTransfer == NULL ||
        cfg->onExit == NULL) {
        return -1;
    }

    handler->cfg = cfg;
    iso14229DownloadHandlerInit(handler);

    self->downloadHandlers[self->nRegisteredDownloadHandlers] = handler;
    self->nRegisteredDownloadHandlers++;
    return 0;
}

typedef struct {
    enum Iso14229DiagnosticServiceId sid;
    void *funcptr;
} ServiceMap;

static const ServiceMap serviceMap[] = {
    {.sid = kSID_DIAGNOSTIC_SESSION_CONTROL, .funcptr = iso14229DiagnosticSessionControl},
    {.sid = kSID_ECU_RESET, .funcptr = iso14229ECUReset},
    {.sid = kSID_READ_DATA_BY_IDENTIFIER, .funcptr = iso14229ReadDataByIdentifier},
    {.sid = kSID_COMMUNICATION_CONTROL, .funcptr = iso14229CommunicationControl},
    {.sid = kSID_WRITE_DATA_BY_IDENTIFIER, .funcptr = iso14229WriteDataByIdentifier},
    {.sid = kSID_ROUTINE_CONTROL, .funcptr = iso14229RoutineControl},
    {.sid = kSID_REQUEST_DOWNLOAD, .funcptr = iso14229RequestDownload},
    {.sid = kSID_TRANSFER_DATA, .funcptr = iso14229TransferData},
    {.sid = kSID_REQUEST_TRANSFER_EXIT, .funcptr = iso14229RequestTransferExit},
    {.sid = kSID_TESTER_PRESENT, .funcptr = iso14229TesterPresent},
};

int iso14229ServerEnableService(Iso14229Server *self, enum Iso14229DiagnosticServiceId sid) {
    for (int i = 0; i < ARRAY_SZ(serviceMap); i++) {
        if (serviceMap[i].sid == sid) {
            if (self->services[sid] == NULL) {
                self->services[sid] = serviceMap[i].funcptr;
                return 0;
            } else {
                return -2;
            }
        }
    }
    return -1;
}
