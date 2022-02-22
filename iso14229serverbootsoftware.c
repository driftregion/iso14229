#include "iso14229serverbootsoftware.h"
#include "iso14229server.h"
#include <stddef.h>
#include <stdint.h>

static enum Iso14229ResponseCode onRequest(void *userCtx, const uint8_t dataFormatIdentifier,
                                           const void *memoryAddress, const size_t memorySize,
                                           uint16_t *maxNumberOfBlockLength) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;

    // ISO-14229-1:2013 Table 397
    if (memorySize > self->cfg->logicalPartitionSize)
    // This boot software implementation disregards the memoryAddress
    // sent by the client.
    // || memoryAddress != self->cfg->logicalPartitionStart)
    {
        return kRequestOutOfRange;
    }

    *maxNumberOfBlockLength = self->cfg->bufferedWriter.pageSize;

    if (0 != bufferedWriterInit(&self->bufferedWriter, &self->cfg->bufferedWriter)) {
        return kGeneralProgrammingFailure;
    }

    return kPositiveResponse;
}

static enum Iso14229ResponseCode onTransfer(void *userCtx, const uint8_t *data, uint32_t len) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;
    bufferedWriterWrite(&self->bufferedWriter, data, len);
    return kPositiveResponse;
}

static enum Iso14229ResponseCode onExit(void *userCtx) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;
    bufferedWriterFinalize(&self->bufferedWriter);
    return kPositiveResponse;
}

enum Iso14229ResponseCode startEraseAppProgramFlashRoutine(void *userCtx,
                                                           Iso14229RoutineControlArgs *args) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)userCtx;
    self->cfg->eraseAppProgramFlash();
    printf("erased flash\n");
    return kPositiveResponse;
}

int udsBootloaderInit(void *vptr_self, const void *vptr_cfg, Iso14229Server *iso14229) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)vptr_self;
    const UDSBootloaderConfig *cfg = (const UDSBootloaderConfig *)vptr_cfg;

    if (NULL == cfg || (cfg->logicalPartitionSize < cfg->bufferedWriter.pageSize) ||
        (NULL == cfg->applicationIsValid) || (NULL == cfg->enterApplication) ||
        (NULL == cfg->eraseAppProgramFlash)) {
        return -1;
    }

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

    self->startup_20ms_timer = iso14229->cfg->userGetms() + 20,
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

    if (0 != iso14229ServerRegisterRoutine(iso14229, &self->eraseAppProgramFlashRoutine)) {
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
static inline int udsBootloaderStateMachine(UDSBootloaderInstance *self, Iso14229Server *iso14229) {
    switch (self->sm_state) {
    case kBootloaderSMStateCheckHasProgrammingRequest:
        // This bootloader doesn't implement external programming requests
        if (false) {
            self->sm_state = kBootloaderSMStateDiagnosticSession;
        } else {
            self->sm_state = kBootloaderSMStateCheckHasValidApp;
        }
        break;

    case kBootloaderSMStateCheckHasValidApp:
        if (true == self->cfg->applicationIsValid()) {
            self->sm_state = kBootloaderSMStateWaitForTesterPresent;
        } else {
            self->sm_state = kBootloaderSMStateDiagnosticSession;
        }
        break;

    // There's a 20ms window here to allow the client to communicate with the
    // bootloader
    case kBootloaderSMStateWaitForTesterPresent:
        if (kDiagModeExtendedDiagnostic == iso14229->diag_mode) {
            self->sm_state = kBootloaderSMStateDiagnosticSession;
        } else if (Iso14229TimeAfter(iso14229->cfg->userGetms(), self->startup_20ms_timer)) {
            self->cfg->enterApplication();
        }
        break;

    // We've heard from the client. Keep a diagnostic session open until the
    // timeout expires.
    case kBootloaderSMStateDiagnosticSession:
        if (Iso14229TimeAfter(iso14229->cfg->userGetms(), iso14229->s3_session_timeout_timer)) {
            // iso14229->cfg->userHardReset();
        }
        break;

    default:
        printf("error. unknown SM state: %d\n", self->sm_state);
        self->sm_state = kBootloaderSMStateCheckHasProgrammingRequest;
        break;
    }
    return 0;
}

int udsBootloaderPoll(void *vptr_self, Iso14229Server *iso14229) {
    UDSBootloaderInstance *self = (UDSBootloaderInstance *)vptr_self;

    return udsBootloaderStateMachine(self, iso14229);
}