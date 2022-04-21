#ifndef ISO14229CLIENT_H
#define ISO14229CLIENT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "iso14229.h"
#include "isotp-c/isotp.h"

#define PRINTHEX(addr, len)                                                                        \
    {                                                                                              \
        for (int i = 0; i < len; i++) {                                                            \
            printf("%02x,", addr[i]);                                                              \
        }                                                                                          \
        printf("\n");                                                                              \
    }

struct Iso14229Client;

typedef struct {
} Iso14229RequestEcuResetArgs;

enum ClientTaskRoutineStatus {
    kRoutineStatusSuccess = 0,
    kRoutineStatusInProgress,
    kRoutineStatusError,
};

/**
 * @brief \~chinese 用户提供的回调函数(使用在工作流当中) \~english User-provided callback function
 * for use in the workflow
 *
 */
typedef enum ClientTaskRoutineStatus(ClientTaskRoutineType)(const struct Iso14229Client *client,
                                                            void *args);

typedef struct {
    enum ClientTaskType {
        kTaskTypeDelay,
        kTaskTypeServiceCall,
        kTaskTypeRoutine,
    } taskType;
    union {
        struct ClientTaskServiceCall {
            enum Iso14229DiagnosticServiceId sid;
        } serviceCall;
        struct ClientTaskDelay {
            uint32_t ms;
        } delay;
        struct ClientTaskRoutine {
            ClientTaskRoutineType *func;
            void *args;
        } routine;
    } type;
} Iso14229ClientTask;

typedef int (*Iso14229UserCANRxPollType)(uint32_t *arb_id, uint8_t *data, uint8_t *size);

typedef struct {
    uint16_t send_id;
    uint16_t recv_id;
    IsoTpLink *link;
    Iso14229ClientTask *tasks;
    uint16_t numtasks;
    uint32_t (*userGetms)();
} Iso14229ClientConfig;

enum Iso14229ClientRequestState {
    kRequestStateIdle = 0,          // 完成
    kRequestStateSending,           // 传输层现在传输数据
    kRequestStateSent,              // 传输层完成传输。可以设置等待计时器
    kRequestStateSentAwaitResponse, // 等待响应
    kRequestStateProcessResponse,   // 处理响应
};

enum Iso14229ClientRequestError {
    kRequestNoError = 0,                 // 没故障
    kRequestNotSentBusy,                 // 在忙、没发
    kRequestNotSentInvalidArgs,          // 参数不对、没发
    kRequestNotSentBufferTooSmall,       // 传输层缓冲器不够大
    kRequestNotSentTransportError,       // 传输层故障、没发
    kRequestTimedOut,                    // 请求超时
    kRequestErrorUnsolicitedResponse,    // 突然响应
    kRequestErrorResponseSIDMismatch,    // 请求和响应SID不一致
    kRequestErrorNegativeResponse,       // 否定响应
    kRequestErrorResponseTransportError, // 传输层故障、响应没有接受
};

struct Iso14229Request {
    union {
        uint8_t *raw;
        struct {
            uint8_t sid;
            union {
                ECUResetRequest ecuReset;
                CommunicationControlRequest communicationControl;
                DiagnosticSessionControlRequest diagnosticSessionControl;
                ReadDataByIdentifierRequest readDataByIdentifier;
                WriteDataByIdentifierRequest writeDataByIdentifier;
                RoutineControlRequest routineControl;
                RequestDownloadRequest requestDownload;
                TransferDataRequest transferData;
                TesterPresentRequest testerPresent;
                ControlDtcSettingRequest controlDtcSetting;
            } __attribute__((packed)) type;
        } * service;
        struct Iso14229GenericRequest {
            uint8_t sid;
            uint8_t subFunction;
            uint8_t data[];
        } * base;
    } as; // points to the ISO-TP send buffer
    uint16_t len;
    uint16_t buffer_size;
};

struct Iso14229ClientSettings {
    uint8_t suppressPositiveResponse;
    struct {
        bool enable;      // send a functional (1:many) request
        uint16_t send_id; // send the functional request to this CAN ID
    } functional;
    uint16_t p2_ms; // Default P2_server_max timing supported by the server for
                    // the activated diagnostic session.
    uint16_t p2_star_ms;
};

typedef struct {
    struct Iso14229Request req;
    struct Iso14229Response resp;
    enum Iso14229ClientRequestState state;
    enum Iso14229ClientRequestError err;
    struct Iso14229ClientSettings settings; // a copy of settings made when a request is sent
} Iso14229ClientRequestContext;

typedef struct Iso14229Client {
    const Iso14229ClientConfig *cfg;
    uint32_t p2_timer;
    Iso14229ClientRequestContext ctx;
    struct Iso14229ClientSettings settings;
} Iso14229Client;

#define DEFAULT_CLIENT_SETTINGS()                                                                  \
    (struct Iso14229ClientSettings) {                                                              \
        .suppressPositiveResponse = false,                                                         \
        .functional =                                                                              \
            {                                                                                      \
                .enable = false,                                                                   \
                .send_id = 0,                                                                      \
            },                                                                                     \
        .p2_ms = 50,                                                                               \
    }

/**
 * @brief Initialize the Iso14229Client
 *
 * @param self
 * @param cfg
 */
void iso14229ClientInit(Iso14229Client *self, const Iso14229ClientConfig *cfg);

typedef int (*UserSequenceFunction)(Iso14229Client *client, void *userContext);

enum Iso14229ClientRequestError ECUReset(Iso14229Client *client,
                                         enum Iso14229ECUResetResetType type);
enum Iso14229ClientRequestError DiagnosticSessionControl(Iso14229Client *client,
                                                         enum Iso14229DiagnosticSessionType mode);
enum Iso14229ClientRequestError SecurityAccess(Iso14229Client *client, uint8_t level, uint8_t *data,
                                               uint16_t size);
enum Iso14229ClientRequestError CommunicationControl(Iso14229Client *client,
                                                     enum Iso14229CommunicationControlType ctrl,
                                                     enum Iso14229CommunicationType comm);
enum Iso14229ClientRequestError ReadDataByIdentifier(Iso14229Client *client,
                                                     const uint16_t *didList,
                                                     const uint16_t numDataIdentifiers);
enum Iso14229ClientRequestError WriteDataByIdentifier(Iso14229Client *client,
                                                      uint16_t dataIdentifier, const uint8_t *data,
                                                      uint16_t size);
enum Iso14229ClientRequestError TesterPresent(Iso14229Client *client);
enum Iso14229ClientRequestError RoutineControl(Iso14229Client *client, enum RoutineControlType type,
                                               uint16_t routineIdentifier, uint8_t *data,
                                               uint16_t size);
enum Iso14229ClientRequestError RequestDownload(Iso14229Client *client,
                                                uint8_t dataFormatIdentifier,
                                                uint8_t addressAndLengthFormatIdentifier,
                                                size_t memoryAddress, size_t memorySize);
enum Iso14229ClientRequestError RequestDownload_32_32(Iso14229Client *client,
                                                      uint32_t memoryAddress, uint32_t memorySize);
enum Iso14229ClientRequestError TransferData(Iso14229Client *client, uint8_t blockSequenceCounter,
                                             const uint16_t blockLength, FILE *fd);
enum Iso14229ClientRequestError RequestTransferExit(Iso14229Client *client);
enum Iso14229ClientRequestError ControlDTCSetting(Iso14229Client *client, uint8_t dtcSettingType,
                                                  uint8_t *dtcSettingControlOptionRecord,
                                                  uint16_t len);

/**
 * @brief Helper function for reading RDBI responses
 *
 * @param did
 * @param data
 * @param size read this many bytes of data
 * @param offset pointer to variable initialized to zero at the first call
 * @return int
 */
static inline int Iso14229ClientRDBIReadU8(const struct Iso14229Response *resp, uint16_t did,
                                           uint8_t *data, uint16_t *offset) {
    // resp->len包含 SID. resp->len减1是服务数据
    // *offset最开始
    if (*offset + sizeof(uint16_t) + sizeof(uint8_t) > resp->len - 1) {
        return -1;
    }
    if (did != Iso14229ntohs(*(uint16_t *)(resp->as.raw + 1 + *offset))) {
        return -2;
    }
    *offset += sizeof(uint16_t);
    *data = *(resp->as.raw + 1 + *offset);
    *offset += sizeof(uint8_t);
    return sizeof(uint8_t);
}

static inline int Iso14229ClientRDBIReadU16(const struct Iso14229Response *resp, uint16_t did,
                                            uint16_t *data, uint16_t *offset) {
    if (*offset + sizeof(uint16_t) + sizeof(uint16_t) > resp->len - 1) {
        return -1;
    }
    if (did != Iso14229ntohs(*(uint16_t *)(resp->as.raw + 1 + *offset))) {
        return -2;
    }
    *offset += sizeof(uint16_t);
    *data = Iso14229ntohs(*(uint16_t *)(resp->as.raw + 1 + *offset));
    *offset += sizeof(uint16_t);
    return sizeof(uint16_t);
}

static inline int Iso14229ClientRDBIReadU32(const struct Iso14229Response *resp, uint16_t did,
                                            uint32_t *data, uint16_t *offset) {
    if (*offset + sizeof(uint16_t) + sizeof(uint16_t) > resp->len - 1) {
        return -1;
    }
    if (did != Iso14229ntohs(*(uint16_t *)(resp->as.raw + 1 + *offset))) {
        return -2;
    }
    *offset += sizeof(uint16_t);
    *data = Iso14229ntohl(*(uint32_t *)(resp->as.raw + 1 + *offset));
    *offset += sizeof(uint32_t);
    return sizeof(uint32_t);
}

/**
 * @brief Poll the client request state machine.
 * @note This is not part of the user API. It is only used internally and for tests.
 *
 * @param self
 */
void Iso14229ClientPoll(Iso14229Client *self);

/**
 * @brief Pass receieved CAN frames to the Iso14229Client.
 * @note This is not part of the user API. It is only used internally and for tests.
 *
 * @param self: pointer to initialized Iso14229Client
 * @param arbitration_id
 * @param data
 * @param size
 */
void Iso14229ClientReceiveCAN(Iso14229Client *self, const uint32_t arbitration_id,
                              const uint8_t *data, const uint8_t size);

#endif
