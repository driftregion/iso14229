#pragma once

#include "sys.h"
#include "config.h"
#include "tp.h"
#include "uds.h"

#define UDS_SUPPRESS_POS_RESP 0x1  // set the suppress positive response bit
#define UDS_FUNCTIONAL 0x2         // send the request as a functional request
#define UDS_IGNORE_SRV_TIMINGS 0x8 // ignore the server-provided p2 and p2_star

/**
 * @brief UDS client structure
 */
typedef struct UDSClient {
    uint16_t p2_ms;      /**< p2 timeout in milliseconds */
    uint32_t p2_star_ms; /**< p2* timeout in milliseconds (for 0x78 response) */
    UDSTp_t *tp;         /**< transport layer handle */

    uint32_t p2_timer;   /**< p2 timer value */
    uint8_t state;       /**< client request state */

    uint8_t options;        /**< current request options */
    uint8_t defaultOptions; /**< default options for all requests */
    uint8_t _options_copy;  /**< copy of options at the time a request is made */

    int (*fn)(struct UDSClient *client, UDSEvent_t evt, void *ev_data); /**< callback function */
    void *fn_data; /**< user-specified function data */

    uint16_t recv_size; /**< size of received data */
    uint16_t send_size; /**< size of data to send */
    uint8_t recv_buf[UDS_CLIENT_RECV_BUF_SIZE]; /**< receive buffer */
    uint8_t send_buf[UDS_CLIENT_SEND_BUF_SIZE]; /**< send buffer */
} UDSClient_t;

/**
 * @brief Security access response structure
 */
struct SecurityAccessResponse {
    uint8_t securityAccessType;      /**< security access type (subfunction) */
    const uint8_t *securitySeed;     /**< pointer to security seed data */
    uint16_t securitySeedLength;     /**< length of security seed */
};

/**
 * @brief Request download response structure
 */
struct RequestDownloadResponse {
    size_t maxNumberOfBlockLength; /**< maximum number of block length */
};

/**
 * @brief Routine control response structure
 */
struct RoutineControlResponse {
    uint8_t routineControlType;         /**< routine control type (subfunction) */
    uint16_t routineIdentifier;         /**< routine identifier */
    const uint8_t *routineStatusRecord; /**< pointer to routine status record */
    uint16_t routineStatusRecordLength; /**< length of routine status record */
};

/**
 * @brief Read data by identifier variable structure
 */
typedef struct {
    uint16_t did;  /**< data identifier */
    uint16_t len;  /**< data length */
    void *data;    /**< pointer to data buffer */
    void *(*UnpackFn)(void *dst, const void *src, size_t n); /**< optional unpack function */
} UDSRDBIVar_t;

UDSErr_t UDSClientInit(UDSClient_t *client);
UDSErr_t UDSClientPoll(UDSClient_t *client);
UDSErr_t UDSSendBytes(UDSClient_t *client, const uint8_t *data, uint16_t size);
UDSErr_t UDSSendECUReset(UDSClient_t *client, uint8_t type);
UDSErr_t UDSSendDiagSessCtrl(UDSClient_t *client, uint8_t mode);
UDSErr_t UDSSendSecurityAccess(UDSClient_t *client, uint8_t level, uint8_t *data, uint16_t size);
UDSErr_t UDSSendCommCtrl(UDSClient_t *client, uint8_t ctrl, uint8_t comm);
UDSErr_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                     const uint16_t numDataIdentifiers);
UDSErr_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                     uint16_t size);
UDSErr_t UDSSendTesterPresent(UDSClient_t *client);
UDSErr_t UDSSendRoutineCtrl(UDSClient_t *client, uint8_t type, uint16_t routineIdentifier,
                            const uint8_t *data, uint16_t size);

UDSErr_t UDSSendRequestDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                                size_t memorySize);

UDSErr_t UDSSendRequestUpload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                              uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                              size_t memorySize);
UDSErr_t UDSSendTransferData(UDSClient_t *client, uint8_t blockSequenceCounter,
                             const uint16_t blockLength, const uint8_t *data, uint16_t size);
UDSErr_t UDSSendTransferDataStream(UDSClient_t *client, uint8_t blockSequenceCounter,
                                   const uint16_t blockLength, FILE *fd);
UDSErr_t UDSSendRequestTransferExit(UDSClient_t *client);

UDSErr_t UDSSendRequestFileTransfer(UDSClient_t *client, uint8_t mode, const char *filePath,
                                    uint8_t dataFormatIdentifier, uint8_t fileSizeParameterLength,
                                    size_t fileSizeUncompressed, size_t fileSizeCompressed);

UDSErr_t UDSCtrlDTCSetting(UDSClient_t *client, uint8_t dtcSettingType,
                           uint8_t *dtcSettingControlOptionRecord, uint16_t len);
UDSErr_t UDSUnpackRDBIResponse(UDSClient_t *client, UDSRDBIVar_t *vars, uint16_t numVars);
UDSErr_t UDSUnpackSecurityAccessResponse(const UDSClient_t *client,
                                         struct SecurityAccessResponse *resp);
UDSErr_t UDSUnpackRequestDownloadResponse(const UDSClient_t *client,
                                          struct RequestDownloadResponse *resp);
UDSErr_t UDSUnpackRoutineControlResponse(const UDSClient_t *client,
                                         struct RoutineControlResponse *resp);

UDSErr_t UDSConfigDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                           uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                           size_t memorySize, FILE *fd);
