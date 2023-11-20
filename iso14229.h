/**
 * @file iso14229.h
 * @brief ISO-14229 (UDS) server and client
 * @author driftregion
 * @version 0.6.0
 * @date 2022-12-08
 */

#ifndef ISO14229_H
#define ISO14229_H

#ifdef __cplusplus
extern "C" {
#define _Static_assert static_assert
#endif

#define UDS_ARCH_CUSTOM 0
#define UDS_ARCH_UNIX 1
#define UDS_ARCH_WINDOWS 2

#define UDS_TP_CUSTOM 0       // bring your own transport layer
#define UDS_TP_ISOTP_C 1      // use isotp-c
#define UDS_TP_ISOTP_SOCKET 2 // use linux ISO-TP socket

#if !defined(UDS_ARCH)
#if defined(__unix__) || defined(__APPLE__)
#define UDS_ARCH UDS_ARCH_UNIX
#elif defined(_WIN32)
#define UDS_ARCH UDS_ARCH_WINDOWS
#else
#define UDS_ARCH UDS_ARCH_CUSTOM
#endif
#endif

#if !defined(UDS_TP)
#if (UDS_ARCH == UDS_ARCH_UNIX)
#define UDS_TP UDS_TP_ISOTP_SOCKET
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
#elif (UDS_TP == UDS_TP_ISOTP_SOCKET)
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
#elif (UDS_ARCH == UDS_ARCH_WINDOWS)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
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

#define UDS_DBG_PRINTHEX(addr, len)                                                                \
    for (int i = 0; i < len; i++) {                                                                \
        UDS_DBG_PRINT("%02x,", ((uint8_t *)addr)[i]);                                              \
    }                                                                                              \
    UDS_DBG_PRINT("\n");

typedef enum {
    UDS_ERR = -1,                 // 通用错误
    UDS_OK = 0,                   // 成功
    UDS_ERR_TIMEOUT,              // 请求超时
    UDS_ERR_NEG_RESP,             // 否定响应
    UDS_ERR_DID_MISMATCH,         // 响应DID对不上期待的DID
    UDS_ERR_SID_MISMATCH,         // 请求和响应SID对不上
    UDS_ERR_SUBFUNCTION_MISMATCH, // 请求和响应SubFunction对不上
    UDS_ERR_TPORT,                // 传输层错误
    UDS_ERR_FILE_IO,              // 文件IO错误
    UDS_ERR_RESP_TOO_SHORT,       // 响应太短
    UDS_ERR_BUFSIZ,               // 缓冲器不够大
    UDS_ERR_INVALID_ARG,          // 参数不对、没发
    UDS_ERR_BUSY,                 // 正在忙、没发
} UDSErr_t;

typedef enum {
    UDSSeqStateDone = 0,
    UDSSeqStateRunning = 1,
    UDSSeqStateGotoNext = 2,
} UDSSeqState_t;

enum UDSDiagnosticSessionType {
    kDefaultSession = 0x01,
    kProgrammingSession = 0x02,
    kExtendedDiagnostic = 0x03,
    kSafetySystemDiagnostic = 0x04,
};

enum {
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



enum UDSTpStatusFlags {
    UDS_TP_IDLE = 0x00000000,
    UDS_TP_SEND_IN_PROGRESS = 0x00000001,
    UDS_TP_RECV_COMPLETE = 0x00000002,
};

typedef uint32_t UDSTpStatus_t;

typedef enum {
    UDS_A_MTYPE_DIAG = 0,
    UDS_A_MTYPE_REMOTE_DIAG,
    UDS_A_MTYPE_SECURE_DIAG,
    UDS_A_MTYPE_SECURE_REMOTE_DIAG,
} UDS_A_Mtype_t;

typedef enum {
    UDS_A_TA_TYPE_PHYSICAL = 0, // unicast (1:1)
    UDS_A_TA_TYPE_FUNCTIONAL,   // multicast
} UDS_A_TA_Type_t;

typedef uint8_t UDSTpAddr_t;

/**
 * @brief Service data unit (SDU)
 * @details data interface between the application layer and the transport layer
 */
typedef struct {
    UDS_A_Mtype_t A_Mtype; // message type (diagnostic, remote diagnostic, secure diagnostic, secure
                           // remote diagnostic)
    uint16_t A_SA;         // application source address
    uint16_t A_TA;         // application target address
    UDS_A_TA_Type_t A_TA_Type; // application target address type (physical or functional)
    uint16_t A_AE;             // application layer remote address
} UDSSDU_t;

#define UDS_TP_NOOP_ADDR (0xFFFFFFFF)

/**
 * @brief Interface to OSI layer 4 (transport layer)
 * @note implementers should embed this struct at offset zero in their own transport layer handle
 */
typedef struct UDSTpHandle {
    /**
     * @brief Get the transport layer's send buffer
     * @param hdl: pointer to transport handle
     * @param buf: double pointer which will be pointed to the send buffer
     * @return size of transport layer's send buffer on success, -1 on error
     */
    ssize_t (*get_send_buf)(struct UDSTpHandle *hdl, uint8_t **p_buf);

    /**
     * @brief Send the data in the buffer buf
     * @param hdl: pointer to transport handle
     * @param buf: a pointer to the data to send (this may be the buffer returned by @ref get_send_buf)
     * @param info: pointer to SDU info (may be NULL). If NULL, implementation should send with physical addressing
     */
    ssize_t (*send)(struct UDSTpHandle *hdl, uint8_t *buf, size_t len, UDSSDU_t *info);
    
    /**
     * @brief Poll the transport layer.
     * @param hdl: pointer to transport handle
     * @note the transport layer user is responsible for calling this function periodically
     * @note threaded implementations like linux isotp sockets don't need to do anything here.
     * @return UDS_TP_IDLE if idle, otherwise UDS_TP_SEND_IN_PROGRESS or UDS_TP_RECV_COMPLETE
     */
    UDSTpStatus_t (*poll)(struct UDSTpHandle *hdl);

    /**
     * @brief Peek at the received data
     * @param hdl: pointer to transport handle
     * @param buf: set to the received data
     * @param info: filled with SDU info by the callee if not NULL
     * @return size of received data on success, -1 on error
     * @note The transport will be unable to receive further data until @ref ack_recv is called
     * @note The information returned by peek will not change until @ref ack_recv is called
     */
    ssize_t (*peek)(struct UDSTpHandle *hdl, uint8_t **buf, UDSSDU_t *info);

    /**
     * @brief Acknowledge that the received data has been processed and may be discarded
     * @param hdl: pointer to transport handle
     * @note: after ack_recv() is called and before new messages are received, peek must return 0. 
     */
    void (*ack_recv)(struct UDSTpHandle *hdl);
} UDSTpHandle_t;

//
// Convenience functions to wrap UDSTpHandle_t
//
ssize_t UDSTpGetSendBuf(UDSTpHandle_t *hdl, uint8_t **buf);
ssize_t UDSTpSend(UDSTpHandle_t *hdl, const uint8_t *buf, ssize_t len, UDSSDU_t *info);
UDSTpStatus_t UDSTpPoll(UDSTpHandle_t *hdl);
ssize_t UDSTpPeek(struct UDSTpHandle *hdl, uint8_t **buf, UDSSDU_t *info);
const uint8_t *UDSTpGetRecvBuf(UDSTpHandle_t *hdl, size_t *len);
size_t UDSTpGetRecvLen(UDSTpHandle_t *hdl);
void UDSTpAckRecv(UDSTpHandle_t *hdl);

// ========================================================================
//                          Utility Functions
// ========================================================================

/* returns true if `a` is after `b` */
static inline bool UDSTimeAfter(uint32_t a, uint32_t b) {
    return ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0);
}

/**
 * @brief Get time in milliseconds
 * @return current time in milliseconds
 */
uint32_t UDSMillis(void);

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

// ========================================================================
//                              Server
// ========================================================================

#ifndef UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS
#define UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS (10)
#endif

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
    UDS_SRV_EVT_ReadMemByAddr,        // UDSReadMemByAddrArgs_t *
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
    UDS_SRV_EVT_DoScheduledReset,     // enum UDSEcuResetType *
    UDS_SRV_EVT_Err,                  // UDSErr_t *
    UDS_EVT_IDLE,
    UDS_EVT_RESP_RECV,
};

typedef int UDSServerEvent_t;
typedef UDSServerEvent_t UDSEvent_t;

/**
 * @brief Server request context
 */
typedef struct {
    uint8_t *recv_buf;
    uint8_t *send_buf;
    size_t recv_len;
    size_t send_len;
    size_t send_buf_size;
    UDSSDU_t info;
} UDSReq_t;

typedef struct UDSServer {
    UDSTpHandle_t *tp;
    uint8_t (*fn)(struct UDSServer *srv, UDSServerEvent_t event, const void *arg);

    /**
     * @brief \~chinese 服务器时间参数（毫秒） \~ Server time constants (milliseconds) \~
     */
    uint16_t p2_ms;      // Default P2_server_max timing supported by the server for
                         // the activated diagnostic session.
    uint32_t p2_star_ms; // Enhanced (NRC 0x78) P2_server_max supported by the
                         // server for the activated diagnostic session.
    uint16_t s3_ms;      // Session timeout

    uint8_t ecuResetScheduled;         // nonzero indicates that an ECUReset has been scheduled
    uint32_t ecuResetTimer;            // for delaying resetting until a response
                                       // has been sent to the client
    uint32_t p2_timer;                 // for rate limiting server responses
    uint32_t s3_session_timeout_timer; // indicates that diagnostic session has timed out

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
    size_t xferBlockLength; // block length (convenience for the TransferData API)

    uint8_t sessionType; // diagnostic session type (0x10)
    uint8_t securityLevel; // SecurityAccess (0x27) level

    bool RCRRP; // set to true when user fn returns 0x78 and false otherwise
    bool requestInProgress; // set to true when a request has been processed but the response has not yet been sent

    // UDS-1 2013 defines the following conditions under which the server does not
    // process incoming requests:
    // - not ready to receive (Table A.1 0x78)
    // - not accepting request messages and not sending responses (9.3.1)
    //
    // when this variable is set to true, incoming ISO-TP data will not be processed.
    bool notReadyToReceive;

    UDSReq_t r;
} UDSServer_t;

// TODO: Remove
typedef struct {
    uint8_t (*fn)(UDSServer_t *srv, UDSServerEvent_t event, const void *arg);
    UDSTpHandle_t *tp;
} UDSServerConfig_t;

typedef struct {
    const uint8_t type;  /*! requested diagnostic session type (enum UDSDiagnosticSessionType) */
    uint16_t p2_ms;      /*! optional: p2 timing override */
    uint32_t p2_star_ms; /*! optional: p2* timing override */
} UDSDiagSessCtrlArgs_t;

typedef struct {
    const uint8_t type; /**< \~chinese 客户端请求的复位类型 \~english reset type requested by client
                           (enum UDSECUResetType) */
    uint32_t powerDownTimeMillis; /**< when this much time has elapsed after a kPositiveResponse, a
                                     UDS_SRV_EVT_DoScheduledReset will be issued */
} UDSECUResetArgs_t;

typedef struct {
    const uint16_t dataId; /*! RDBI Data Identifier */
    uint8_t (*copy)(UDSServer_t *srv , const void *src, uint16_t count); /*! function for copying data */
} UDSRDBIArgs_t;

typedef struct {
    const void *memAddr;
    const size_t memSize;
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */
} UDSReadMemByAddrArgs_t;

typedef struct {
    uint8_t ctrlType; /* enum UDSCommunicationControlType */
    uint8_t commType; /* enum UDSCommunicationType */
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
    const uint16_t dataId;     /*! WDBI Data Identifier */
    const uint8_t *const data; /*! pointer to data */
    const uint16_t len;        /*! length of data */
} UDSWDBIArgs_t;

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
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestDownloadArgs_t;

typedef struct {
    const void *addr;                   /*! requested address */
    const size_t size;                  /*! requested download size */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestUploadArgs_t;

typedef struct {
    const uint8_t *const data; /*! transfer data */
    const uint16_t len;        /*! transfer data length */
    const uint16_t maxRespLen; /*! don't send more than this many bytes with copyResponse */
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

UDSErr_t UDSServerInit(UDSServer_t *srv);
void UDSServerPoll(UDSServer_t *srv);

#ifdef __cplusplus
}
#endif

#endif
