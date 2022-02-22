#ifndef BOOTSOFTWARE_H
#define BOOTSOFTWARE_H

/**
 * @file bootsoftware.h
 * @brief a boot software implementation loosely based on ISO14229-1-2013 Figure 38
 *
 */

#include "iso14229server.h"
#include "iso14229serverbufferedwriter.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief UDS Bootloader configuration struct
 *
 */
typedef struct {
    /**
     * @brief Does a user-defined check on the program flash logical partition
     * to see if it's a valid application that should be booted. This probably
     * involves a CRC calculation.
     * @return true if the application can be booted.
     */
    bool (*applicationIsValid)();

    /**
     * @brief no-return function for entering the application
     */
    void (*enterApplication)();

    /**
     * @brief erases the program flash logical partition
     */
    void (*eraseAppProgramFlash)();

    BufferedWriterConfig bufferedWriter;

    size_t logicalPartitionSize; // total logical partition size in bytes
} UDSBootloaderConfig;

enum UDSBootloaderStateMachineStateEnum {
    kBootloaderSMStateCheckHasProgrammingRequest = 0,
    kBootloaderSMStateCheckHasValidApp,
    kBootloaderSMStateWaitForTesterPresent,
    kBootloaderSMStateDiagnosticSession,
};

typedef struct {
    const UDSBootloaderConfig *cfg;

    uint32_t startup_20ms_timer; // bootloader上电20ms之后
    enum UDSBootloaderStateMachineStateEnum sm_state;

    /**
     * @brief Implementation of functions necessary for 0x34 RequestDownload,
     * 0x36 TransferData, 0x37 RequestTransferExit
     */
    Iso14229DownloadHandlerConfig dlHandlerCfg;
    Iso14229DownloadHandler dlHandler;

    /**
     * @brief Implementation of 0x31 RoutineControl for erasing app program
     * FLASH
     */
    Iso14229Routine eraseAppProgramFlashRoutine;

    BufferedWriter bufferedWriter;
} UDSBootloaderInstance;

int udsBootloaderInit(void *self, const void *cfg, Iso14229Server *iso14229);
int udsBootloaderPoll(void *self, Iso14229Server *iso14229);

#define ISO14229_MIDDLEWARE_BOOTSOFTWARE(self_obj, cfg_obj)                                        \
    {                                                                                              \
        .self = &self_obj, .cfg = &cfg_obj, .initFunc = udsBootloaderInit,                         \
        .pollFunc = udsBootloaderPoll                                                              \
    }

#endif
