#ifndef ISO14229SERVER_H
#define ISO14229SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "isotp-c/isotp.h"
#include "iso14229.h"
#include "iso14229serverconfig.h"

typedef struct Iso14229Server Iso14229Server;
struct Iso14229ServerStatus;

/**
 * @addtogroup 0x31_routineControl
 */
typedef struct {
    const uint8_t *optionRecord;
    const uint16_t optionRecordLength;

    uint8_t *statusRecord;
    const uint16_t statusRecordBufferSize;
    uint16_t *statusRecordLength;
} Iso14229RoutineControlArgs;

enum Iso14229AddressingScheme {
    kAddressingSchemePhysical = 0, // 1:1
    kAddressingSchemeFunctional,   // 1:many
};

/**
 * @brief Service Request Context
 */
typedef struct {
    struct {
        const uint8_t *buf;
        const uint16_t len; // size of request data
        const enum Iso14229AddressingScheme addressingScheme;
    } req;
    struct Iso14229Response resp;
} Iso14229ServerRequestContext;

typedef enum Iso14229ResponseCode (*Iso14229Service)(Iso14229Server *self,
                                                     Iso14229ServerRequestContext *req);

/**
 * @brief User-Defined handler for 0x34 RequestDownload, 0x36 TransferData, and
 * 0x37 RequestTransferExit
 *
 */
typedef struct {
    // ISO14229-1-2013: 14.4.2.3, Table 404: The blockSequenceCounter parameter
    // value starts at 0x01
    uint8_t blockSequenceCounter;

    /**
     * @brief ISO14229-1-2013: Table 407 - 0x36 TransferData Supported negative
     * response codes requires that the server keep track of whether the
     * transfer is active
     */
    bool isActive;

    size_t requestedTransferSize; // total transfer size in bytes requested by the client
    size_t numBytesTransferred;   // total number of bytes transferred

    void *userCtx;                   // optional: pointer to context
    uint16_t maxNumberOfBlockLength; // mandatory: server's maximum allowable size of data received
                                     // in onTransfer(...)

    /**
     * @brief mandatory: callback function to process transferred data
     * @param status
     * @param userCtx
     * @param data
     * @param len
     * @return 0x0 kPositiveResponse: the transfer was accepted
     * @return 0x78 kRequestCorrectlyReceived_ResponsePending: the transfer was accepted. Notify the
     * client before calling this function again
     */
    enum Iso14229ResponseCode (*onTransfer)(const struct Iso14229ServerStatus *status,
                                            void *userCtx, const uint8_t *data, uint32_t len);
    /**
     * @brief optional: callback function to complete transfer
     * @param status
     * @param userCtx
     * @param buffer_size size of response buffer in bytes.
     * @param transferResponseParameterRecord response buffer
     * @param transferResponseParameterRecordSize pointer to size of response
     */
    enum Iso14229ResponseCode (*onExit)(const struct Iso14229ServerStatus *status, void *userCtx,
                                        uint16_t buffer_size,
                                        uint8_t *transferResponseParameterRecord,
                                        uint16_t *transferResponseParameterRecordSize);

} Iso14229DownloadHandler;

static inline void Iso14229DownloadHandlerInit(Iso14229DownloadHandler *handler,
                                               size_t memorySize) {
    assert(handler);
    assert(handler->onTransfer);
    handler->requestedTransferSize = memorySize;
    handler->blockSequenceCounter = 1;
    handler->numBytesTransferred = 0;
}

typedef struct {
    uint16_t phys_recv_id;
    uint16_t func_recv_id;
    uint16_t send_id;
    IsoTpLink *phys_link;
    IsoTpLink *func_link;

    uint8_t *receive_buffer;
    uint16_t receive_buf_size;
    uint8_t *send_buffer;
    uint16_t send_buf_size;

    /**
     * @brief ~\chinese 用户定义诊断会话控制回调函数~\english user-provided DiagnosticSessionControl
     * handler \~
     * @addtogroup diagnosticSessionControl_0x10
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 positiveResponse
     *  \li 0x12 subFunctionNotSupported
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 conditionsNotCorrect
     */
    enum Iso14229ResponseCode (*userDiagnosticSessionControlHandler)(
        const struct Iso14229ServerStatus *status, enum Iso14229DiagnosticSessionType type);

    /**
     * @brief \~chinese 用户定义ECU复位请求处理函数。
      \~english user-provided function handle an ECU reset request.
     * \~
     * @addtogroup ecuReset_0x11
     * @param status 当前服务器状态
     * @param resetType 复位类型
     * @return
     肯定响应意味着复位已经安排好了。服务器回停止处理新数据。建议用户停止发送所有CAN报文。用户应该等到CAN物理层响应发送完成后进行复位。
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 positiveResponse
     *  \li 0x12 sub-functionNotSupported
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 kConditionsNotCorrect
     *  \li 0x33 kSecurityAccessDenied
     */
    enum Iso14229ResponseCode (*userECUResetHandler)(const struct Iso14229ServerStatus *status,
                                                     uint8_t resetType, uint8_t *powerDownTime);

    /**
     * @brief ~\chinese 用户定义读取标识符指定数据回调函数 ~\english user-provided RDBI handler \~
     * @addtogroup readDataByIdentifier_0x22
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 positiveResponse
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 conditionsNotCorrect
     *  \li 0x31 requestOutOfRange
     *  \li 0x33 securityAccessDenied
     */
    enum Iso14229ResponseCode (*userRDBIHandler)(const struct Iso14229ServerStatus *status,
                                                 uint16_t dataId, const uint8_t **data_location,
                                                 uint16_t *len);
    /**
     * @brief ~\chinese 用户定义写入标识符指定数据回调函数 ~\english user-provided WDBI handler. ~\
     * @addtogroup writeDataByIdentifier_0x2E
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 positiveResponse
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 conditionsNotCorrect
     *  \li 0x31 requestOutOfRange
     *  \li 0x33 securityAccessDenied
     *  \li 0x72 generalProgrammingFailure
     */
    enum Iso14229ResponseCode (*userWDBIHandler)(const struct Iso14229ServerStatus *status,
                                                 uint16_t dataId, const uint8_t *data,
                                                 uint16_t len);

    /**
     * @brief \~chinese 用户定义CommunicationControl回调函数 \~english user-provided
     * CommunicationControl handler. ~\
     * @addtogroup communicationControl_0x28
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 kPositiveResponse
     *  \li 0x12 sub-functionNotSupported
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 conditionsNotCorrect
     *  \li 0x31 requestOutOfRange
     *
     */
    enum Iso14229ResponseCode (*userCommunicationControlHandler)(
        const struct Iso14229ServerStatus *status, uint8_t controlType, uint8_t communicationType);

    /**
     * @brief \~chinese 用户定义安全访问回调函数 \~english user-provided SecurityAccess handler. \~
     * @addtogroup securityAccess_0x27
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 positiveResponse
     *  \li 0x12 sub-functionNotSupported
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 kConditionsNotCorrect
     *  \li 0x24 kRequestSequenceError
     *  \li 0x31 kRequestOutOfRange
     *  \li 0x35 kInvalidKey
     *  \li 0x36 kExceededNumberOfAttempts
     *  \li 0x37 kRequiredTimeDelayNotExpired
     */
    enum Iso14229ResponseCode (*userSecurityAccessGenerateSeed)(
        const struct Iso14229ServerStatus *status, uint8_t level, const uint8_t *in_data,
        uint16_t in_size, uint8_t *out_data, uint16_t out_bufsize, uint16_t *out_size);

    enum Iso14229ResponseCode (*userSecurityAccessValidateKey)(
        const struct Iso14229ServerStatus *status, uint8_t level, const uint8_t *key,
        uint16_t size);

    /**
     * @brief
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 positiveResponse
     *  \li 0x12 subFunctionNotSupported
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 conditionsNotCorrect
     *  \li 0x24 requestSequenceError
     *  \li 0x31 requestOutOfRange
     *  \li 0x33 securityAccessDenied
     *  \li 0x72 generalProgrammingFailure
     *
     */
    enum Iso14229ResponseCode (*userRoutineControlHandler)(
        const struct Iso14229ServerStatus *status, enum RoutineControlType routineControlType,
        uint16_t routineIdentifier, Iso14229RoutineControlArgs *args);

    /**
     * @brief
     * @param memoryAddress
     * @param memorySize
     * @param dataFormatIdentifier
     * @param handler set this pointer to the address of an Iso14229DownloadHandler instance
     * @param maxNumberOfBlockLength inform the client how many data bytes to include. You must set
     * this to a value greater than or equal to 3. This is usually related to the size of your
     * receive buffer.
     * @return one of [kPositiveResponse, kRequestOutOfRange]
     */
    enum Iso14229ResponseCode (*userRequestDownloadHandler)(
        const struct Iso14229ServerStatus *status, void *memoryAddress, size_t memorySize,
        uint8_t dataFormatIdentifier, Iso14229DownloadHandler **handler,
        uint16_t *maxNumberOfBlockLength);

    /**
    * @brief \~chinese 会话超时处理函数。这个函数应该立刻进行ECU复位。
    \~english user-provided session timeout callback. This function should reset the ECU
    immediately.
     * \~
     */
    void (*userSessionTimeoutCallback)();

    /**
     * @brief \~chinese 用户定义获取时间（毫秒）回调函数 \~english user-provided function that
     * returns the current time in milliseconds \~
     *
     */
    uint32_t (*userGetms)();

    /**
     * @brief \~chinese 服务器时间参数（毫秒） \~ Server time constants (milliseconds) \~
     */
    uint16_t p2_ms;      // Default P2_server_max timing supported by the server for
                         // the activated diagnostic session.
    uint16_t p2_star_ms; // Enhanced (NRC 0x78) P2_server_max supported by the
                         // server for the activated diagnostic session.
    uint16_t s3_ms;      // Session timeout
} Iso14229ServerConfig;

/**
 * @brief Iso14229 root struct
 *
 */
typedef struct Iso14229Server {
    const Iso14229ServerConfig *cfg;

    Iso14229Service services[kISO14229_SID_NOT_SUPPORTED];

    // The active download handler. NULL indicates that there is not currently a download in
    // progress.
    Iso14229DownloadHandler *downloadHandler;

    bool ecuResetScheduled;            // indicates that an ECUReset has been scheduled
    uint32_t ecuResetTimer;            // for delaying resetting until a response
                                       // has been sent to the client
    uint32_t p2_timer;                 // for rate limiting server responses
    uint32_t s3_session_timeout_timer; // for knowing when the diagnostic
                                       // session has timed out
    uint16_t receive_size;             // number of bytes received from ISO-TP layer

    /**
     * @brief public subset of server state for user handlers
     */
    struct Iso14229ServerStatus {
        enum Iso14229DiagnosticSessionType sessionType;
        uint8_t securityLevel; // Current SecurityAccess (0x27) level
        // this variable set to true when a user handler returns 0x78
        // requestCorrectlyReceivedResponsePending. After a response has been sent on the transport
        // layer, this variable is set to false and the user handler will be called again. It is the
        // responsibility of the user handler to track the call count.
        bool RCRRP;
    } status;

    // ISO14229-1 2013 defines the following conditions under which the server does not
    // process incoming requests:
    // - not ready to receive (Table A.1 0x78)
    // - not accepting request messages and not sending responses (9.3.1)
    //
    // when this variable is set to true, incoming ISO-TP data will not be processed.
    bool notReadyToReceive;

} Iso14229Server;

// ========================================================================
//                              User Functions
// ========================================================================

void Iso14229ServerInit(Iso14229Server *self, const Iso14229ServerConfig *cfg);

/**
 * @brief Poll the iso14229 object and its children. Call this function
 * periodically.
 *
 * @param self: pointer to initialized Iso14229Server
 */
void Iso14229ServerPoll(Iso14229Server *self);

/**
 * @brief Pass receieved CAN frames to the Iso14229Server
 *
 * @param self: pointer to initialized Iso14229Server
 * @param arbitration_id
 * @param data
 * @param size
 */
void iso14229ServerReceiveCAN(Iso14229Server *self, const uint32_t arbitration_id,
                              const uint8_t *data, const uint8_t size);

/**
 * @brief User-implemented CAN send function
 *
 * @param arbitration_id
 * @param data
 * @param size
 * @return uint32_t
 */

enum Iso14229BootManagerSMState {
    kBootManagerSMStateWaitForProgrammingRequest = 0, // 等待外部下载请求
    kBootManagerSMStateReprogramming,                 // 收到了下载请求或者应用不得行
};

struct Iso14229BootManager {
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

    enum Iso14229BootManagerSMState sm_state;

    Iso14229Server *srv;

    uint32_t extRequestWindowTimer; // 外部请求等待时间计时器
};

/**
 * @brief
 *
 * @param mgr
 * @param srv
 * @param applicationIsValid
 * @param enterApplication
 * @param extRequestWindowTimems
 */
static inline void Iso14229BootManagerInit(struct Iso14229BootManager *mgr,
                                           struct Iso14229Server *srv, bool (*applicationIsValid)(),
                                           void (*enterApplication)(),
                                           uint32_t extRequestWindowTimems) {
    assert(mgr);
    assert(srv);
    assert(applicationIsValid);
    assert(enterApplication);
    mgr->applicationIsValid = applicationIsValid;
    mgr->enterApplication = enterApplication;
    mgr->extRequestWindowTimer = srv->cfg->userGetms() + extRequestWindowTimems;
    mgr->srv = srv;
}

/**
 * @brief
 *
 * @param mgr
 */
static inline void Iso14229BootManagerPoll(struct Iso14229BootManager *mgr) {
    switch (mgr->sm_state) {
    case kBootManagerSMStateWaitForProgrammingRequest:
        if (Iso14229TimeAfter(mgr->srv->cfg->userGetms(), mgr->extRequestWindowTimer)) {
            if (mgr->applicationIsValid()) {
                mgr->enterApplication();
            } else {
                mgr->sm_state = kBootManagerSMStateReprogramming;
            }
        } else if (kDefaultSession != mgr->srv->status.sessionType) {
            mgr->sm_state = kBootManagerSMStateReprogramming;
        }
        break;
    case kBootManagerSMStateReprogramming:
        break;
    default:
        assert(0);
    }
}

#endif