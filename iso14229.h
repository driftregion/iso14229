/**
 * @file iso14229.h
 * @brief ISO-14229 (UDS) server and client
 * @author driftregion
 * @version 0.5.0
 * @date 2022-12-08
 */

#ifndef ISO14229_H
#define ISO14229_H

#ifdef __cplusplus
extern "C" {
#endif

#define UDS_ARCH_CUSTOM 0
#define UDS_ARCH_UNIX 1

#define UDS_TP_CUSTOM 0
#define UDS_TP_ISOTP_C 1
#define UDS_TP_LINUX_SOCKET 2

#if !defined(UDS_ARCH)
#if defined(__unix__) || defined(__APPLE__)
#define UDS_ARCH UDS_ARCH_UNIX
#endif
#endif

#if !defined(UDS_TP)
#if (UDS_ARCH == UDS_ARCH_UNIX)
#define UDS_TP UDS_TP_LINUX_SOCKET
#endif
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#if (UDS_TP == UDS_TP_ISOTP_C)
#include "isotp-c/isotp.h"
#include "isotp-c/isotp_config.h"
#include "isotp-c/isotp_defines.h"
#include "isotp-c/isotp_user.h"
#elif (UDS_TP == UDS_TP_LINUX_SOCKET)
#include <errno.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if (UDS_ARCH == UDS_ARCH_UNIX)
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#endif

/** ISO-TP Maximum Transmissiable Unit (ISO-15764-2-2004 section 5.3.3) */
#define UDS_ISOTP_MTU (4095)

#ifndef UDS_TP_MTU
#define UDS_TP_MTU UDS_ISOTP_MTU
#endif

/** Default buffer size */
#define UDS_BUFSIZE UDS_TP_MTU

/*
provide a debug function with -DUDS_DBG_PRINT=printf when compiling this
library
*/
#ifndef UDS_DBG_PRINT
#define UDS_DBG_PRINT(fmt, ...) ((void)fmt)
#endif

typedef int UDSErr_t;

enum {
    UDS_ERR = -1,
    UDS_OK = 0,
    UDS_ERR_TPORT = 1000,
};

enum UDSClientError {

    kUDS_CLIENT_ERR_RESP_TPORT_ERR = -14,      // 传输层故障、无法接收
    kUDS_CLIENT_ERR_REQ_NOT_SENT_EOF = -13,    // 没发：FILE没有数据
    kUDS_CLIENT_ERR_RESP_SCHEMA_INVALID = -12, // 数据内容或者大小不按照应用定义(如ODX)
    kUDS_CLIENT_ERR_RESP_DID_MISMATCH = -11,            // 响应DID对不上期待的DID
    kUDS_CLIENT_ERR_RESP_CANNOT_UNPACK = -10,           // 响应不能解析
    kUDS_CLIENT_ERR_RESP_TOO_SHORT = -9,                // 响应太小
    kUDS_CLIENT_ERR_RESP_NEGATIVE = -8,                 // 否定响应
    kUDS_CLIENT_ERR_RESP_SID_MISMATCH = -7,             // 请求和响应SID对不上
    kUDS_CLIENT_ERR_RESP_UNEXPECTED = -6,               // 突然响应
    kUDS_CLIENT_ERR_REQ_TIMED_OUT = -5,                 // 请求超时
    kUDS_CLIENT_ERR_REQ_NOT_SENT_TPORT_ERR = -4,        // 传输层故障、没发
    kUDS_CLIENT_ERR_REQ_NOT_SENT_BUF_TOO_SMALL = -3,    // 传输层缓冲器不够大
    kUDS_CLIENT_ERR_REQ_NOT_SENT_INVALID_ARGS = -2,     // 参数不对、没发
    kUDS_CLIENT_ERR_REQ_NOT_SENT_SEND_IN_PROGRESS = -1, // 在忙、没发
    kUDS_CLIENT_OK = 0,                                 // 流程完成
};

typedef int UDSClientError_t;

enum UDSSequenceError {
    kUDS_SEQ_ERR_FERROR = -4,        // ferror()文件故障
    kUDS_SEQ_ERR_NULL_CALLBACK = -3, // 回调函数是NULL
    kUDS_SEQ_ERR_CLIENT_ERR = -2,    // 因为Client故障而停止
    kUDS_SEQ_FAIL = -1,              // 通用故障
    kUDS_SEQ_COMPLETE = 0,           // 完成成功
    kUDS_SEQ_RUNNING = 1,            // 流程正在跑、还没完成
    kUDS_SEQ_ADVANCE = 2,            // 流程正在跑、还没完成
};

typedef int UDSSequenceError_t;

enum UDSDiagnosticSessionType {
    kDefaultSession = 0x01,
    kProgrammingSession = 0x02,
    kExtendedDiagnostic = 0x03,
    kSafetySystemDiagnostic = 0x04,
};

enum UDSServerResponseCode {
    kPositiveResponse = 0,
    kGeneralReject = 0x10,
    kServiceNotSupported = 0x11,
    kSubFunctionNotSupported = 0x12,
    kIncorrectMessageLengthOrInvalidFormat = 0x13,
    kResponseTooLong = 0x14,
    kBusyRepeatRequest = 0x21,
    kConditionsNotCorrect = 0x22,
    kRequestSequenceError = 0x24,
    kNoResponseFromSubnetComponent = 0x25,
    kFailurePreventsExecutionOfRequestedAction = 0x26,
    kRequestOutOfRange = 0x31,
    kSecurityAccessDenied = 0x33,
    kInvalidKey = 0x35,
    kExceedNumberOfAttempts = 0x36,
    kRequiredTimeDelayNotExpired = 0x37,
    kUploadDownloadNotAccepted = 0x70,
    kTransferDataSuspended = 0x71,
    kGeneralProgrammingFailure = 0x72,
    kWrongBlockSequenceCounter = 0x73,
    kRequestCorrectlyReceived_ResponsePending = 0x78,
    kSubFunctionNotSupportedInActiveSession = 0x7E,
    kServiceNotSupportedInActiveSession = 0x7F,
    kRpmTooHigh = 0x81,
    kRpmTooLow = 0x82,
    kEngineIsRunning = 0x83,
    kEngineIsNotRunning = 0x84,
    kEngineRunTimeTooLow = 0x85,
    kTemperatureTooHigh = 0x86,
    kTemperatureTooLow = 0x87,
    kVehicleSpeedTooHigh = 0x88,
    kVehicleSpeedTooLow = 0x89,
    kThrottlePedalTooHigh = 0x8A,
    kThrottlePedalTooLow = 0x8B,
    kTransmissionRangeNotInNeutral = 0x8C,
    kTransmissionRangeNotInGear = 0x8D,
    kISOSAEReserved = 0x8E,
    kBrakeSwitchNotClosed = 0x8F,
    kShifterLeverNotInPark = 0x90,
    kTorqueConverterClutchLocked = 0x91,
    kVoltageTooHigh = 0x92,
    kVoltageTooLow = 0x93,
};

/**
 * @brief LEV_RT_
 * @addtogroup ecuReset_0x11
 */
enum UDSECUResetType {
    kHardReset = 1,
    kKeyOffOnReset = 2,
    kSoftReset = 3,
    kEnableRapidPowerShutDown = 4,
    kDisableRapidPowerShutDown = 5,
};

typedef uint8_t UDSECUReset_t;

/**
 * @addtogroup securityAccess_0x27
 */
enum UDSSecurityAccessType {
    kRequestSeed = 0x01,
    kSendKey = 0x02,
};

/**
 * @addtogroup communicationControl_0x28
 */
enum UDSCommunicationControlType {
    kEnableRxAndTx = 0,
    kEnableRxAndDisableTx = 1,
    kDisableRxAndEnableTx = 2,
    kDisableRxAndTx = 3,
};

/**
 * @addtogroup communicationControl_0x28
 */
enum UDSCommunicationType {
    kNormalCommunicationMessages = 0x1,
    kNetworkManagementCommunicationMessages = 0x2,
    kNetworkManagementCommunicationMessagesAndNormalCommunicationMessages = 0x3,
};

/**
 * @addtogroup routineControl_0x31
 */
enum RoutineControlType {
    kStartRoutine = 1,
    kStopRoutine = 2,
    kRequestRoutineResults = 3,
};

/**
 * @addtogroup controlDTCSetting_0x85
 */
enum DTCSettingType {
    kDTCSettingON = 0x01,
    kDTCSettingOFF = 0x02,
};

enum UDSTpAddr {
    kTpAddrTypePhysical = 0, // 1:1
    kTpAddrTypeFunctional,   // 1:many
};

typedef uint8_t UDSTpAddr_t;

enum UDSTpStatusFlags {
    TP_SEND_INPROGRESS = 0x00000001,
};

typedef uint32_t UDSTpStatus_t;

/**
 * @brief 传输层把柄
 */
typedef struct UDSTpHandle {
    /**
     * @brief 接收
     * @param hdl: pointer to transport handle
     * @param buf: if data is available, it will be copied here
     * @param count: the implementation should not copy more than count bytes into buf
     * @param ta_type: the addressing type of the data received or error encountered is written to
     * this location. If no data is received, nothing is written
     * @return 接收了多少个字节。
     * < 0 故障
     * == 0 没有数据
     * > 0 接收了、返回值是大小
     */
    ssize_t (*recv)(struct UDSTpHandle *hdl, void *buf, size_t count, UDSTpAddr_t *ta_type);
    /**
     * @brief 发送
     * @param hdl: pointer to transport handle
     * @param buf: pointer to data to be sent
     * @param count: number of bytes to be sent
     * @param ta_type: the addressing type to use
     * @return 发送了多少个字节。
     * < 0 故障
     * >= 0 发送成功了
     */
    ssize_t (*send)(struct UDSTpHandle *hdl, const void *buf, size_t count, UDSTpAddr_t ta_type);
    /**
     * @brief 轮询
     */
    UDSTpStatus_t (*poll)(struct UDSTpHandle *hdl);
    void *impl; // opaque pointer to transport implementation
} UDSTpHandle_t;

#if UDS_TP == UDS_TP_ISOTP_C
typedef struct {
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t func_recv_buf[8];
    uint8_t func_send_buf[8];
} UDSTpIsoTpC_t;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
typedef struct {
    int phys_fd;
    int func_fd;
} UDSTpLinuxIsoTp_t;
#endif

// ========================================================================
//                          Utility Functions
// ========================================================================

/* returns true if `a` is after `b` */
static inline bool UDSTimeAfter(uint32_t a, uint32_t b) {
    return ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0);
}

/**
 * @brief \~chinese 用户定义获取时间（毫秒）回调函数 \~english user-provided function that
 * returns the current time in milliseconds \~
 */
uint32_t UDSMillis();

// ========================================================================
//                              Client
// ========================================================================

#ifndef UDS_CLIENT_DEFAULT_P2_MS
#define UDS_CLIENT_DEFAULT_P2_MS (150U)
#endif

#ifndef UDS_CLIENT_DEFAULT_P2_STAR_MS
#define UDS_CLIENT_DEFAULT_P2_STAR_MS (1500U)
#endif

_Static_assert(UDS_CLIENT_DEFAULT_P2_STAR_MS > UDS_CLIENT_DEFAULT_P2_MS, "");

enum UDSClientRequestState {
    kRequestStateIdle = 0,          // 完成
    kRequestStateSending,           // 传输层现在传输数据
    kRequestStateAwaitSendComplete, // 等待传输发送完成
    kRequestStateAwaitResponse,     // 等待响应
    kRequestStateProcessResponse,   // 处理响应
};

typedef uint8_t UDSClientRequestState_t;

typedef struct {
#if UDS_TP == UDS_TP_CUSTOM
    UDSTpHandle_t *tp;
#elif UDS_TP == UDS_TP_ISOTP_C
    uint16_t phys_recv_id;
    uint16_t phys_send_id;
    uint16_t func_send_id;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    const char *if_name;
    uint16_t phys_recv_id;
    uint16_t phys_send_id;
    uint16_t func_send_id;
#else
#error "transport undefined"
#endif
} UDSClientConfig_t;

enum UDSClientOptions {
    SUPPRESS_POS_RESP = 0x1,     // 服务器不应该发送肯定响应
    FUNCTIONAL = 0x2,            // 发功能请求
    NEG_RESP_IS_ERR = 0x4,       // 否定响应是属于故障
    IGNORE_SERVER_TIMINGS = 0x8, // 忽略服务器给的p2和p2_star
};

struct UDSClient;
struct UDSSequence;

/**
 * @brief
 */
typedef UDSSequenceError_t (*UDSClientCallback)(struct UDSClient *client,
                                                struct UDSSequence *sequence);

/**
 * @brief Convenience macro to allow inheritance of UDSSequence_t by specific sequences
 */
#define UDS_SEQUENCE_STRUCT_MEMBERS                                                                \
    /*! null-terminated list of callback functions */                                              \
    const UDSClientCallback *cbList;                                                               \
    /*! index of currently active callback function */                                             \
    size_t cbIdx;                                                                                  \
    /*! error code to be set by sequence functions */                                              \
    UDSSequenceError_t err;                                                                        \
    /*! name of active callback function */                                                        \
    const char *funcName;                                                                          \
    /*! optional pointer to a function that is called when the active callback function changes*/  \
    void (*onChange)(struct UDSSequence * sequence);

/**
 * @brief A linear series of functions that make UDS requests and handle responses
 */
typedef struct UDSSequence {
    UDS_SEQUENCE_STRUCT_MEMBERS;
} UDSSequence_t;

typedef struct {
    UDS_SEQUENCE_STRUCT_MEMBERS;
    uint8_t dataFormatIdentifier;
    uint8_t addressAndLengthFormatIdentifier;
    size_t memoryAddress;
    size_t memorySize;
    FILE *fd;
    uint8_t blockSequenceCounter;
    uint16_t blockLength;
} UDSClientDownloadSequence_t;

typedef struct UDSClient {
    uint16_t p2_ms;      // p2 超时时间
    uint32_t p2_star_ms; // 0x78 p2* 超时时间
    UDSTpHandle_t *tp;
    uint32_t (*TimeNow_ms)();

    // 内状态
    uint32_t p2_timer;
    uint8_t recv_buf[UDS_BUFSIZE];
    uint8_t send_buf[UDS_BUFSIZE];
    uint16_t recv_buf_size;
    uint16_t send_buf_size;
    uint16_t recv_size;
    uint16_t send_size;
    UDSClientError_t err;
    UDSClientRequestState_t state;

    uint8_t options;        // enum udsclientoptions
    uint8_t defaultOptions; // enum udsclientoptions
    // a copy of the options at the time a request is made
    uint8_t _options_copy; // enum udsclientoptions

#if UDS_TP == UDS_TP_CUSTOM
#elif UDS_TP == UDS_TP_ISOTP_C
    UDSTpHandle_t _tp_hdl;
    UDSTpIsoTpC_t tp_impl;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    UDSTpHandle_t _tp_hdl;
    UDSTpLinuxIsoTp_t tp_impl;
#endif

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

UDSErr_t UDSClientInit(UDSClient_t *client, const UDSClientConfig_t *cfg);

void UDSClientDeInit(UDSClient_t *client);

/**
 * @brief Prefer using the higher-level UDSSequencePoll instead. This function is used in tests
 * and internally
 * @param client
 */
void UDSClientPoll(UDSClient_t *client);

UDSClientError_t UDSSendECUReset(UDSClient_t *client, UDSECUReset_t type);
UDSClientError_t UDSSendDiagSessCtrl(UDSClient_t *client, enum UDSDiagnosticSessionType mode);
UDSClientError_t UDSSendSecurityAccess(UDSClient_t *client, uint8_t level, uint8_t *data,
                                       uint16_t size);
UDSClientError_t UDSSendCommCtrl(UDSClient_t *client, enum UDSCommunicationControlType ctrl,
                                 enum UDSCommunicationType comm);
UDSClientError_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                             const uint16_t numDataIdentifiers);
UDSClientError_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                             uint16_t size);
UDSClientError_t UDSSendTesterPresent(UDSClient_t *client);
UDSClientError_t UDSSendRoutineCtrl(UDSClient_t *client, enum RoutineControlType type,
                                    uint16_t routineIdentifier, const uint8_t *data, uint16_t size);

UDSClientError_t UDSSendRequestDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                        uint8_t addressAndLengthFormatIdentifier,
                                        size_t memoryAddress, size_t memorySize);

UDSClientError_t UDSSendRequestUpload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                      uint8_t addressAndLengthFormatIdentifier,
                                      size_t memoryAddress, size_t memorySize);
UDSClientError_t UDSSendTransferData(UDSClient_t *client, uint8_t blockSequenceCounter,
                                     const uint16_t blockLength, const uint8_t *data,
                                     uint16_t size);
UDSClientError_t UDSSendTransferDataStream(UDSClient_t *client, uint8_t blockSequenceCounter,
                                           const uint16_t blockLength, FILE *fd);
UDSClientError_t UDSSendRequestTransferExit(UDSClient_t *client);

UDSClientError_t UDSCtrlDTCSetting(UDSClient_t *client, uint8_t dtcSettingType,
                                   uint8_t *dtcSettingControlOptionRecord, uint16_t len);
UDSClientError_t UDSUnpackRDBIResponse(const UDSClient_t *client, uint16_t did, uint8_t *data,
                                       uint16_t size, uint16_t *offset);
UDSClientError_t UDSUnpackSecurityAccessResponse(const UDSClient_t *client,
                                                 struct SecurityAccessResponse *resp);
UDSClientError_t UDSUnpackRequestDownloadResponse(const UDSClient_t *client,
                                                  struct RequestDownloadResponse *resp);
UDSClientError_t UDSUnpackRoutineControlResponse(const UDSClient_t *client,
                                                 struct RoutineControlResponse *resp);

/**
 * @brief Run a client sequence
 *
 * @param client
 * @param sequence
 * @return UDSClientError_t
 * - < 0 : error
 * - > 0 : no error -- sequence in progress
 * - == 0 : no error -- sequence complete
 * @example
 *
 */
UDSClientError_t UDSSequencePoll(UDSClient_t *client, UDSSequence_t *sequence);

void UDSSequenceInit(UDSSequence_t *sequence, const UDSClientCallback *cbList,
                     void (*onChange)(UDSSequence_t *sequence));

/**
 * @brief Wait after request transmission for a response to be received
 * @note if suppressPositiveResponse is set, this function will return
 kUDS_SEQ_ADVANCE as soon as the transport layer has completed transmission.
 *
 * @param client
 * @param args
 * @return UDSClientError_t
    - kUDS_SEQ_COMPLETE -- 流程完成
    - kUDS_SEQ_RUNNING  -- 流程正在跑、还没完成
 */
UDSClientError_t UDSClientAwaitIdle(UDSClient_t *client, UDSSequence_t *seq);

UDSClientError_t UDSConfigDownload(UDSClientDownloadSequence_t *sequence,
                                   uint8_t dataFormatIdentifier,
                                   uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                                   size_t memorySize, FILE *fd);

// ========================================================================
//                              Server
// ========================================================================

#ifndef UDS_SERVER_DEFAULT_P2_MS
#define UDS_SERVER_DEFAULT_P2_MS (50)
#endif

#ifndef UDS_SERVER_DEFAULT_P2_STAR_MS
#define UDS_SERVER_DEFAULT_P2_STAR_MS (2000)
#endif

#ifndef UDS_SERVER_DEFAULT_S3_MS
#define UDS_SERVER_DEFAULT_S3_MS (3000)
#endif

_Static_assert(0 < UDS_SERVER_DEFAULT_P2_MS &&
                   UDS_SERVER_DEFAULT_P2_MS < UDS_SERVER_DEFAULT_P2_STAR_MS &&
                   UDS_SERVER_DEFAULT_P2_STAR_MS < UDS_SERVER_DEFAULT_S3_MS,
               "");

#ifndef UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH
/*! ISO14229-1:2013 Table 396. This parameter is used by the requestDownload positive response
message to inform the client how many data bytes (maxNumberOfBlockLength) to include in each
TransferData request message from the client. */
#define UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH (UDS_BUFSIZE)
#endif

enum UDSServerEvent {
    UDS_SRV_EVT_DiagSessCtrl,         // UDSDiagSessCtrlArgs_t *
    UDS_SRV_EVT_EcuReset,             // UDSECUResetArgs_t *
    UDS_SRV_EVT_ReadDataByIdent,      // UDSRDBIArgs_t *
    UDS_SRV_EVT_CommCtrl,             // UDSCommCtrlArgs_t *
    UDS_SRV_EVT_SecAccessRequestSeed, // UDSSecAccessRequestSeedArgs_t *
    UDS_SRV_EVT_SecAccessValidateKey, // UDSSecAccessValidateKeyArgs_t *
    UDS_SRV_EVT_WriteDataByIdent,     // UDSWDBIArgs_t *
    UDS_SRV_EVT_RoutineCtrl,          // UDSRoutineCtrlArgs_t*
    UDS_SRV_EVT_RequestDownload,      // UDSRequestDownloadArgs_t*
    UDS_SRV_EVT_RequestUpload,        // UDSRequestUploadArgs_t *
    UDS_SRV_EVT_TransferData,         // UDSTransferDataArgs_t *
    UDS_SRV_EVT_RequestTransferExit,  // UDSRequestTransferExitArgs_t *
    UDS_SRV_EVT_SessionTimeout,       // NULL
    UDS_SRV_EVT_PowerDown,            // UDSPowerDownArgs_t *
};

typedef int UDSServerEvent_t;

typedef struct UDSServer {
    UDSTpHandle_t *tp;
    uint8_t (*fn)(struct UDSServer *srv, UDSServerEvent_t event, const void *arg);
    uint8_t recv_buf[UDS_BUFSIZE];
    uint8_t send_buf[UDS_BUFSIZE];
    uint16_t recv_size;
    uint16_t send_size;
    uint16_t recv_buf_size;
    uint16_t send_buf_size;

    /**
     * @brief \~chinese 服务器时间参数（毫秒） \~ Server time constants (milliseconds) \~
     */
    uint16_t p2_ms;      // Default P2_server_max timing supported by the server for
                         // the activated diagnostic session.
    uint32_t p2_star_ms; // Enhanced (NRC 0x78) P2_server_max supported by the
                         // server for the activated diagnostic session.
    uint16_t s3_ms;      // Session timeout

    bool ecuResetScheduled;            // indicates that an ECUReset has been scheduled
    uint32_t ecuResetTimer;            // for delaying resetting until a response
                                       // has been sent to the client
    uint32_t p2_timer;                 // for rate limiting server responses
    uint32_t s3_session_timeout_timer; // for knowing when the diagnostic
                                       // session has timed out

    /**
     * @brief UDS-1-2013: Table 407 - 0x36 TransferData Supported negative
     * response codes requires that the server keep track of whether the
     * transfer is active
     */
    bool xferIsActive;
    // UDS-1-2013: 14.4.2.3, Table 404: The blockSequenceCounter parameter
    // value starts at 0x01
    uint8_t xferBlockSequenceCounter;
    size_t xferTotalBytes;  // total transfer size in bytes requested by the client
    size_t xferByteCounter; // total number of bytes transferred

    /**
     * @brief public subset of server state for user handlers
     */
    uint8_t sessionType;
    uint8_t securityLevel; // Current SecurityAccess (0x27) level
    // this variable set to true when a user handler returns 0x78
    // requestCorrectlyReceivedResponsePending. After a response has been sent on the transport
    // layer, this variable is set to false and the user handler will be called again. It is the
    // responsibility of the user handler to track the call count.
    bool RCRRP;

    // UDS-1 2013 defines the following conditions under which the server does not
    // process incoming requests:
    // - not ready to receive (Table A.1 0x78)
    // - not accepting request messages and not sending responses (9.3.1)
    //
    // when this variable is set to true, incoming ISO-TP data will not be processed.
    bool notReadyToReceive;

#if UDS_TP == UDS_TP_CUSTOM
#elif UDS_TP == UDS_TP_ISOTP_C
    UDSTpHandle_t _tp_hdl;
    UDSTpIsoTpC_t tp_impl;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    UDSTpHandle_t _tp_hdl;
    UDSTpLinuxIsoTp_t tp_impl;
#endif
} UDSServer_t;

typedef struct {
    uint8_t (*fn)(UDSServer_t *srv, UDSServerEvent_t event, const void *arg);
#if UDS_TP == UDS_TP_CUSTOM
    UDSTpHandle_t *tp;
#elif UDS_TP == UDS_TP_ISOTP_C
    uint16_t phys_send_id;
    uint16_t phys_recv_id;
    uint16_t func_recv_id;
#elif UDS_TP == UDS_TP_LINUX_SOCKET
    const char *if_name;
    uint16_t phys_send_id;
    uint16_t phys_recv_id;
    uint16_t func_recv_id;
#else
#error "transport undefined"
#endif
} UDSServerConfig_t;

typedef struct {
    const enum UDSDiagnosticSessionType type; /*! requested session type */
    uint16_t p2_ms;                           /*! optional: p2 timing override */
    uint32_t p2_star_ms;                      /*! optional: p2* timing override */
} UDSDiagSessCtrlArgs_t;

typedef struct {
    const enum UDSECUResetType
        type; /**< \~chinese 客户端请求的复位类型 \~english reset type requested by client */
    uint8_t powerDownTime; /**< Optional response: notify client of time until shutdown (0-254) 255
                              indicates that a time is not available. */
} UDSECUResetArgs_t;

typedef struct {
    const enum UDSECUResetType
        type; /**< \~chinese 客户端请求的复位类型 \~english reset type requested by client */
} UDSPowerDownArgs_t;

typedef struct {
    const uint16_t dataId; /*! RDBI Data Identifier */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */
} UDSRDBIArgs_t;

typedef struct {
    const uint16_t dataId;     /*! WDBI Data Identifier */
    const uint8_t *const data; /*! pointer to data */
    const uint16_t len;        /*! length of data */
} UDSWDBIArgs_t;

typedef struct {
    enum UDSCommunicationControlType ctrlType;
    enum UDSCommunicationType commType;
} UDSCommCtrlArgs_t;

typedef struct {
    const uint8_t level;             /*! requested security level */
    const uint8_t *const dataRecord; /*! pointer to request data */
    const uint16_t len;              /*! size of request data */
    uint8_t (*copySeed)(UDSServer_t *srv, const void *src,
                        uint16_t len); /*! function for copying data */
} UDSSecAccessRequestSeedArgs_t;

typedef struct {
    const uint8_t level;      /*! security level to be validated */
    const uint8_t *const key; /*! key sent by client */
    const uint16_t len;       /*! length of key */
} UDSSecAccessValidateKeyArgs_t;

typedef struct {
    const uint8_t ctrlType;      /*! routineControlType */
    const uint16_t id;           /*! routineIdentifier */
    const uint8_t *optionRecord; /*! optional data */
    const uint16_t len;          /*! length of optional data */
    uint8_t (*copyStatusRecord)(UDSServer_t *srv, const void *src,
                                uint16_t len); /*! function for copying response data */
} UDSRoutineCtrlArgs_t;

typedef struct {
    const void *addr;                   /*! requested address */
    const size_t size;                  /*! requested download size */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    uint16_t maxNumberOfBlockLength; /*! response: inform client how many data bytes to send in each
                                        `TransferData` request */
} UDSRequestDownloadArgs_t;

typedef struct {
    const uint8_t *const data; /*! transfer data */
    const uint16_t len;        /*! transfer data length */
    uint8_t (*copyResponse)(
        UDSServer_t *srv, const void *src,
        uint16_t len); /*! function for copying transfer data response data (optional) */
} UDSTransferDataArgs_t;

typedef struct {
    const uint8_t *const data; /*! request data */
    const uint16_t len;        /*! request data length */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len); /*! function for copying response data (optional) */
} UDSRequestTransferExitArgs_t;

UDSErr_t UDSServerInit(UDSServer_t *self, const UDSServerConfig_t *cfg);
void UDSServerDeInit(UDSServer_t *self);
void UDSServerPoll(UDSServer_t *self);

#ifdef __cplusplus
}
#endif

#endif
