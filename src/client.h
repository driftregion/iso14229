#pragma once

#include "sys.h"
#include "tp.h"
#include "uds.h"

#define UDS_SUPPRESS_POS_RESP 0x1  // set the suppress positive response bit
#define UDS_FUNCTIONAL 0x2         // send the request as a functional request
#define UDS_IGNORE_SRV_TIMINGS 0x8 // ignore the server-provided p2 and p2_star

typedef struct UDSClient {
    uint16_t p2_ms;      // p2 超时时间
    uint32_t p2_star_ms; // 0x78 p2* 超时时间
    UDSTp_t *tp;

    uint32_t p2_timer;
    uint8_t *recv_buf;
    uint8_t *send_buf;
    uint16_t recv_buf_size;
    uint16_t send_buf_size;
    uint16_t recv_size;
    uint16_t send_size;
    uint8_t state; // client request state

    uint8_t options;
    uint8_t defaultOptions;
    // a copy of the options at the time a request is made
    uint8_t _options_copy;

    // callback function
    int (*fn)(struct UDSClient *client, UDSEvent_t evt, void *ev_data);
    void *fn_data; // user-specified function data
} UDSClient_t;

struct SecurityAccessResponse {
    uint8_t securityAccessType;
    const uint8_t *securitySeed;
    uint16_t securitySeedLength;
};

struct RequestDownloadResponse {
    size_t maxNumberOfBlockLength;
};

struct RoutineControlResponse {
    uint8_t routineControlType;
    uint16_t routineIdentifier;
    const uint8_t *routineStatusRecord;
    uint16_t routineStatusRecordLength;
};

typedef struct {
    uint16_t did;
    uint16_t len;
    void *data;
    void *(*UnpackFn)(void *dst, const void *src, size_t n);
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
