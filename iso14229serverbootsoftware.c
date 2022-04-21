#include "iso14229serverbootsoftware.h"
#include "iso14229.h"
#include "iso14229server.h"
#include "iso14229serverbufferedwriter.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

static enum Iso14229ResponseCode onRequest(const struct Iso14229ServerStatus *status, void *userCtx,
                                           const uint8_t dataFormatIdentifier,
                                           const void *memoryAddress, const size_t memorySize,
                                           uint16_t *maxNumberOfBlockLength) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;

    if (self->transferInProgress) {
        return kConditionsNotCorrect;
    }

    // ISO-14229-1:2013 Table 397
    if (memorySize > self->cfg->logicalPartitionSize)
    // This boot software implementation disregards the memoryAddress
    // sent by the client.
    // || memoryAddress != self->cfg->logicalPartitionStart)
    {
        return kRequestOutOfRange;
    }

    *maxNumberOfBlockLength = self->cfg->bufferedWriter.pageBufferSize;

    if (0 != bufferedWriterInit(&self->bufferedWriter, &self->cfg->bufferedWriter)) {
        return kGeneralProgrammingFailure;
    }

    self->requestedWriteSize = memorySize;
    self->transferInProgress = true;
    self->numBytesTransferred = 0;

    return kPositiveResponse;
}

static enum Iso14229ResponseCode onTransfer(const struct Iso14229ServerStatus *status,
                                            void *userCtx, const uint8_t *data, uint32_t len) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;

    if (false == self->transferInProgress) {
        return kRequestSequenceError;
    }

    if (BufferedWriterProcess(&self->bufferedWriter, data, len)) {
        return kRequestCorrectlyReceived_ResponsePending;
    } else {
        self->numBytesTransferred += len;
        if (self->numBytesTransferred > self->requestedWriteSize) {
            self->transferInProgress = false;
            return kTransferDataSuspended;
        }
        return kPositiveResponse;
    }
}

static enum Iso14229ResponseCode onExit(const struct Iso14229ServerStatus *status, void *userCtx) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;
    if (false == self->transferInProgress) {
        return kRequestSequenceError;
    }

    if (self->numBytesTransferred != self->requestedWriteSize) {
        self->transferInProgress = false;
        return kRequestSequenceError;
    }

    if (BufferedWriterProcess(&self->bufferedWriter, NULL, 0)) {
        return kRequestCorrectlyReceived_ResponsePending;
    } else {
        self->transferInProgress = false;
        return kPositiveResponse;
    }
}

enum Iso14229ResponseCode
startEraseAppProgramFlashRoutine(const struct Iso14229ServerStatus *status, void *userCtx,
                                 Iso14229RoutineControlArgs *args) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;
    if (status->RCRRP) {
        self->cfg->eraseAppProgramFlash();
        return kPositiveResponse;
    } else {
        return kRequestCorrectlyReceived_ResponsePending;
    }
}

int udsBootloaderInit(void *vptr_self, const void *vptr_cfg, Iso14229Server *iso14229) {
    assert(vptr_self);
    assert(vptr_cfg);
    assert(iso14229);
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)vptr_self;
    const UDSBootloaderConfig *cfg = (const UDSBootloaderConfig *)vptr_cfg;

    if (NULL == cfg || (cfg->logicalPartitionSize < cfg->bufferedWriter.pageBufferSize) ||
        (NULL == cfg->applicationIsValid) || (NULL == cfg->enterApplication) ||
        (NULL == cfg->eraseAppProgramFlash)) {
        return -1;
    }
    memset(self, 0, sizeof(*self));

    self->cfg = cfg;

    // ISO14229-1:2013 15.3.1.1.2 Boot software diagnostic service requirements
    iso14229ServerEnableService(iso14229, kSID_DIAGNOSTIC_SESSION_CONTROL);
    iso14229ServerEnableService(iso14229, kSID_COMMUNICATION_CONTROL);
    iso14229ServerEnableService(iso14229, kSID_ROUTINE_CONTROL);
    iso14229ServerEnableService(iso14229, kSID_ECU_RESET);
    iso14229ServerEnableService(iso14229, kSID_READ_DATA_BY_IDENTIFIER);
    iso14229ServerEnableService(iso14229, kSID_WRITE_DATA_BY_IDENTIFIER);
    iso14229ServerEnableService(iso14229, kSID_REQUEST_DOWNLOAD);
    iso14229ServerEnableService(iso14229, kSID_TRANSFER_DATA);
    iso14229ServerEnableService(iso14229, kSID_REQUEST_TRANSFER_EXIT);
    iso14229ServerEnableService(iso14229, kSID_TESTER_PRESENT);

    self->extRequestWindowTimer = iso14229->cfg->userGetms() + self->cfg->extRequestWindowTimems,
    self->sm_state = kBootloaderSMStateCheckHasProgrammingRequest;

    self->eraseAppProgramFlashRoutine = (Iso14229Routine){
        .routineIdentifier = 0xFF00,
        .startRoutine = startEraseAppProgramFlashRoutine,
        .stopRoutine = NULL,
        .requestRoutineResults = NULL,
        .userCtx = self,
    };

    self->dlHandlerCfg = (Iso14229DownloadHandlerConfig){
        .onRequest = onRequest,
        .onTransfer = onTransfer,
        .onExit = onExit,
        .userCtx = self,
    };

    if (0 != Iso14229ServerRegisterRoutine(iso14229, &self->eraseAppProgramFlashRoutine)) {
        return -1;
    }

    if (0 !=
        iso14229ServerRegisterDownloadHandler(iso14229, &self->dlHandler, &self->dlHandlerCfg)) {
        return -1;
    }

    return 0;
}

/**
 * @brief Example UDS Boot Software state machine per ISO14229-1-2013: Figure 38
 *
 * @param self
 */
void UDSBootloaderPoll(void *vptr_self, Iso14229Server *iso14229) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)vptr_self;

    switch (self->sm_state) {
    case kBootloaderSMStateCheckHasProgrammingRequest:
        if (Iso14229TimeAfter(iso14229->cfg->userGetms(), self->extRequestWindowTimer)) {
            self->sm_state = kBootloaderSMStateCheckHasValidApp;
        } else if (kDefaultSession != iso14229->status.sessionType) {
            self->sm_state = kBootloaderSMStateDone;
        }
        break;
    case kBootloaderSMStateCheckHasValidApp:
        if (self->cfg->applicationIsValid()) {
            self->cfg->enterApplication();
        } else {
            self->sm_state = kBootloaderSMStateDone;
        }
        break;
    case kBootloaderSMStateDone:
        break;
    default:
        assert(0);
    }
}