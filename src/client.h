#pragma once

#include "sys.h"
#include "tp.h"
#include "uds.h"

enum UDSClientRequestState {
    kRequestStateIdle = 0,          // 完成
    kRequestStateSending,           // 传输层现在传输数据
    kRequestStateAwaitSendComplete, // 等待传输发送完成
    kRequestStateAwaitResponse,     // 等待响应
    kRequestStateProcessResponse,   // 处理响应
};

typedef uint8_t UDSClientRequestState_t;

enum UDSClientOptions {
    UDS_SUPPRESS_POS_RESP = 0x1,  // 服务器不应该发送肯定响应
    UDS_FUNCTIONAL = 0x2,         // 发功能请求
    UDS_NEG_RESP_IS_ERR = 0x4,    // 否定响应是属于故障
    UDS_IGNORE_SRV_TIMINGS = 0x8, // 忽略服务器给的p2和p2_star
};

struct UDSClient;

typedef UDSSeqState_t (*UDSClientCallback)(struct UDSClient *client);

typedef struct UDSClient {
    uint16_t p2_ms;      // p2 超时时间
    uint32_t p2_star_ms; // 0x78 p2* 超时时间
    UDSTpHandle_t *tp;

    // 内状态
    uint32_t p2_timer;
    uint8_t *recv_buf;
    uint8_t *send_buf;
    uint16_t recv_buf_size;
    uint16_t send_buf_size;
    uint16_t recv_size;
    uint16_t send_size;
    UDSErr_t err;
    UDSClientRequestState_t state;

    uint8_t options;        // enum udsclientoptions
    uint8_t defaultOptions; // enum udsclientoptions
    // a copy of the options at the time a request is made
    uint8_t _options_copy; // enum udsclientoptions
    int (*fn)(struct UDSClient *, int, void *, void *);

    const UDSClientCallback *cbList; // null-terminated list of callback functions
    size_t cbIdx;                    // index of currently active callback function
    void *cbData;                    // a pointer to data available to callbacks

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

UDSErr_t UDSClientInit(UDSClient_t *client);

#define UDS_CLIENT_IDLE (0)
#define UDS_CLIENT_RUNNING (1)

/**
 * @brief poll the client (call this in a loop)
 * @param client
 * @return UDS_CLIENT_IDLE if idle, otherwise UDS_CLIENT_RUNNING
 */
bool UDSClientPoll(UDSClient_t *client);
void UDSClientPoll2(UDSClient_t *client,
                    int (*fn)(UDSClient_t *client, int evt, void *ev_data, void *fn_data),
                    void *fn_data);

UDSErr_t UDSSendBytes(UDSClient_t *client, const uint8_t *data, uint16_t size);
UDSErr_t UDSSendECUReset(UDSClient_t *client, UDSECUReset_t type);
UDSErr_t UDSSendDiagSessCtrl(UDSClient_t *client, enum UDSDiagnosticSessionType mode);
UDSErr_t UDSSendSecurityAccess(UDSClient_t *client, uint8_t level, uint8_t *data, uint16_t size);
UDSErr_t UDSSendCommCtrl(UDSClient_t *client, enum UDSCommunicationControlType ctrl,
                         enum UDSCommunicationType comm);
UDSErr_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                     const uint16_t numDataIdentifiers);
UDSErr_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                     uint16_t size);
UDSErr_t UDSSendTesterPresent(UDSClient_t *client);
UDSErr_t UDSSendRoutineCtrl(UDSClient_t *client, enum RoutineControlType type,
                            uint16_t routineIdentifier, const uint8_t *data, uint16_t size);

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

UDSErr_t UDSSendRequestFileTransfer(UDSClient_t *client, enum FileOperationMode mode, const char *filePath, 
                                uint8_t dataFormatIdentifier, uint8_t fileSizeParameterLength, 
                                size_t fileSizeUncompressed, size_t fileSizeCompressed);

UDSErr_t UDSCtrlDTCSetting(UDSClient_t *client, uint8_t dtcSettingType,
                           uint8_t *dtcSettingControlOptionRecord, uint16_t len);
UDSErr_t UDSUnpackRDBIResponse(const uint8_t *buf, size_t buf_len, uint16_t did, uint8_t *data,
                               uint16_t size, uint16_t *offset);
UDSErr_t UDSUnpackSecurityAccessResponse(const UDSClient_t *client,
                                         struct SecurityAccessResponse *resp);
UDSErr_t UDSUnpackRequestDownloadResponse(const UDSClient_t *client,
                                          struct RequestDownloadResponse *resp);
UDSErr_t UDSUnpackRoutineControlResponse(const UDSClient_t *client,
                                         struct RoutineControlResponse *resp);

/**
 * @brief Wait after request transmission for a response to be received
 * @note if suppressPositiveResponse is set, this function will return
 UDSSeqStateGotoNext as soon as the transport layer has completed transmission.
 *
 * @param client
 * @param args
 * @return UDSErr_t
    - UDSSeqStateDone -- 流程完成
    - UDSSeqStateRunning  -- 流程正在跑、还没完成
 */
UDSSeqState_t UDSClientAwaitIdle(UDSClient_t *client);

UDSErr_t UDSConfigDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                           uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                           size_t memorySize, FILE *fd);
