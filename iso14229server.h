#ifndef ISO14229SERVER_H
#define ISO14229SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
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

/**
 * @addtogroup routineControl_0x31
 */
typedef enum Iso14229ResponseCode (*Iso14229RoutineControlUserCallbackType)(
    const struct Iso14229ServerStatus *status, void *userCtx, Iso14229RoutineControlArgs *args);

/**
 * @addtogroup routineControl_0x31
 */
typedef struct Iso14229Routine {
    uint16_t routineIdentifier; // Table 378 — Request message definition [0-0xFFFF]
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
    Iso14229RoutineControlUserCallbackType startRoutine;
    Iso14229RoutineControlUserCallbackType stopRoutine;
    Iso14229RoutineControlUserCallbackType requestRoutineResults;
    void *userCtx; // Pointer to user data
} Iso14229Routine;

enum Iso14229AddressingScheme {
    kAddressingSchemePhysical = 0, // 1:1
    kAddressingSchemeFunctional,   // 1:many
};

/**
 * @brief Service Request Context
 */
typedef struct {
    struct {
        union {
            const uint8_t *raw; // request data (excluding service ID)
            const TesterPresentRequest *testerPresent;
            const ControlDtcSettingRequest *controlDtcSetting;
            // const DiagnosticSessionControlRequest *diagnosticSessionControl;
            // const ECUResetRequest *ecuReset;
            const SecurityAccessRequest *securityAccess;
            const CommunicationControlRequest *communicationControl;
            const ReadDataByIdentifierRequest *readDataByIdentifier;
            const WriteDataByIdentifierRequest *writeDataByIdentifier;
            const RoutineControlRequest *routineControl;
            const RequestDownloadRequest *requestDownload;
            const TransferDataRequest *transferData;
            // const RequestTransferExitRequest *requestTransferExit;
        } as;
        const uint16_t len; // size of request data (excluding service ID)
        const uint8_t sid;  // service ID
        const enum Iso14229AddressingScheme addressingScheme;
    } req;
    struct Iso14229Response resp;
} Iso14229ServerRequestContext;

typedef enum Iso14229ResponseCode (*Iso14229Service)(Iso14229Server *self,
                                                     const Iso14229ServerRequestContext *req);

/**
 * @brief User-Defined handler for 0x34 RequestDownload, 0x36 TransferData, and
 * 0x37 RequestTransferExit
 *
 */
typedef struct {
    /**
     * @brief
     * @param maxNumberOfBlockLength maximum chunk size that the client can
     * accept in bytes
     * @return one of [kPositiveResponse, kRequestOutOfRange]
     */
    enum Iso14229ResponseCode (*onRequest)(const struct Iso14229ServerStatus *status, void *userCtx,
                                           const uint8_t dataFormatIdentifier,
                                           const void *memoryAddress, const size_t memorySize,
                                           uint16_t *maxNumberOfBlockLength);
    enum Iso14229ResponseCode (*onTransfer)(const struct Iso14229ServerStatus *status,
                                            void *userCtx, const uint8_t *data, uint32_t len);
    enum Iso14229ResponseCode (*onExit)(const struct Iso14229ServerStatus *status, void *userCtx);

    void *userCtx;
} Iso14229DownloadHandlerConfig;

typedef struct {
    const Iso14229DownloadHandlerConfig *cfg;

    // ISO14229-1-2013: 14.4.2.3, Table 404: The blockSequenceCounter parameter
    // value starts at 0x01
    uint8_t blockSequenceCounter;

    /**
     * @brief ISO14229-1-2013: Table 407 - 0x36 TransferData Supported negative
     * response codes requires that the server keep track of whether the
     * transfer is active
     */
    bool isActive;
} Iso14229DownloadHandler;

/**
 * @brief UserMiddleware: an interface for extending iso14299
 * @note See appsoftware.h and bootsoftware.h for examples
 */
typedef struct {
    /**
     * @brief pointer to a user-defined middleware object. This can be NULL
     *
     */
    void *self;

    /**
     * @brief const pointer to a user-defined middleware configuration object. This can be NULL.
     *
     */
    const void *cfg;

    /**
     * @brief user defined middleware initialization function. This can be NULL
     * @note This gets called once inside Iso14229UserInit(...).
     * This is intended to perform additional configuration the iso14229
     * instance such as enabling services, implementing RoutineControl or Upload
     * Download functional unit handlers.
     */
    int (*initFunc)(void *self, const void *cfg, struct Iso14229Server *iso14229);

    /**
     * @brief user defined middleware polling function. This can be NULL
     * @note this gets called inside Iso14229UserPoll(...).
     * It is indended to run middleware-specific time-dependent
     * state-machine behavior which can modify the state of the middleware or
     * the iso14229 instance
     */
    void (*pollFunc)(void *self, struct Iso14229Server *iso14229);
} Iso14229UserMiddleware;

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
     * @brief ~\chinese 用户定义读取标识符指定数据回调函数 ~\english user-provided RDBI handler \~
     * @addtogroup readDataByIdentifier_0x22
     * @details \~chinese 允许响应: \~english Permitted responses: \~
     *  \li 0x00 positiveResponse
     *  \li 0x13 incorrectMessageLengthOrInvalidFormat
     *  \li 0x22 conditionsNotCorrect
     *  \li 0x31 requestOutOfRange
     *  \li 0x33 securityAccessDenied
     */
    enum Iso14229ResponseCode (*userRDBIHandler)(uint16_t dataId, const uint8_t **data_location,
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
    enum Iso14229ResponseCode (*userWDBIHandler)(uint16_t dataId, const uint8_t *data,
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
    enum Iso14229ResponseCode (*userCommunicationControlHandler)(uint8_t controlType,
                                                                 uint8_t communicationType);

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
    enum Iso14229ResponseCode (*userSecurityAccessGenerateSeed)(uint8_t level,
                                                                const uint8_t *in_data,
                                                                uint16_t in_size, uint8_t *out_data,
                                                                uint16_t out_bufsize,
                                                                uint16_t *out_size);

    enum Iso14229ResponseCode (*userSecurityAccessValidateKey)(uint8_t level, const uint8_t *key,
                                                               uint16_t size);

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
                                                     uint8_t resetType);

    /**
    * @brief \~chinese 会话超时处理函数。这个函数应该立刻进行ECU复位。
    \~english user-provided session timeout handler. This function should probably reset the ECU
    immediately.
     * \~
     */
    void (*userSessionTimeoutHandler)();

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

    uint16_t ecuResetDelayms; // time to delay between

    Iso14229UserMiddleware *middleware;
} Iso14229ServerConfig;

/**
 * @brief Iso14229 root struct
 *
 */
typedef struct Iso14229Server {
    const Iso14229ServerConfig *cfg;

    Iso14229Service services[ISO14229_MAX_DIAGNOSTIC_SERVICES];
    const Iso14229Routine *routines[ISO14229_SERVER_MAX_ROUTINES]; // 0x31 RoutineControl
    uint16_t nRegisteredRoutines;

    Iso14229DownloadHandler *downloadHandlers[ISO14229_SERVER_MAX_DOWNLOAD_HANDLERS];
    uint16_t nRegisteredDownloadHandlers;

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

/**
 * @brief Enable the requested service. Services are disabled by default
 *
 * @param self
 * @param sid
 * @return int 0: success, -1: service not found, -2:service already installed
 */
int iso14229ServerEnableService(Iso14229Server *self, enum Iso14229DiagnosticServiceId sid);

/**
 * @brief Register a 0x31 RoutineControl routine
 *
 * @param self
 * @param routine
 * @return int 0: success
 */
int Iso14229ServerRegisterRoutine(Iso14229Server *self, const Iso14229Routine *routine);

/**
 * @brief Register a handler for the sequence [0x34 RequestDownload, 0x36
 * TransferData, 0x37 RequestTransferExit]
 *
 * @param self
 * @param handler
 * @param cfg
 * @return int 0: success
 */
int iso14229ServerRegisterDownloadHandler(Iso14229Server *self, Iso14229DownloadHandler *handler,
                                          Iso14229DownloadHandlerConfig *cfg);

#endif
