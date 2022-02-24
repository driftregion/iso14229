#ifndef ISO14229CLIENT_H
#define ISO14229CLIENT_H

#include <stdbool.h>
#include <stdint.h>
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

/**
 * @brief \~chinese 客户端请求参数 \~english Client request arguments
 *
 */
typedef struct {
    // union {
    //     struct {
    //         enum Iso14229DiagnosticMode diagnosticSessionType;
    //     } diagnosticSessionControl;
    //     struct {
    //         enum Iso14229ECUResetResetType resetType;
    //     } ecuReset;
    //     struct {
    //         uint16_t *dataIdentifierList;
    //         uint16_t numDataIdentifiers;
    //     } readDataByIdentifier;
    //     struct {
    //         enum RoutineControlType routineControlType;
    //         uint16_t routineIdentifier;
    //         uint8_t *routineControlOptionRecord;
    //         uint16_t routineControlOptionRecordLength;
    //     } routineControl;
    // } type;

    // uint8_t suppressPositiveResponse;
    // struct {
    //     bool enable;      // send a functional (1:many) request
    //     uint16_t send_id; // send the functional request to this CAN ID
    // } functional;

} Iso14229RequestArgs;

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
            Iso14229RequestArgs args;
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
    kRequestStateSentAwaitResponse, // 等待响应
};

enum Iso14229ClientRequestError {
    kRequestNoError = 0,              // 没故障
    kRequestNotSentBusy,              // 在忙、没发
    kRequestNotSentInvalidArgs,       // 参数不对、没发
    kRequestNotSentBufferTooSmall,    // 传输层缓冲器不够大
    kRequestNotSentTransportError,    // 传输层故障、没发
    kRequestTimedOut,                 // 请求超时
    kRequestErrorUnsolicitedResponse, // 突然响应
    kRequestErrorResponseSIDMismatch, // 请求和响应SID不一致
    kRequestErrorNegativeResponse,    // 否定响应
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
            } __attribute__((packed)) type;
        } * service;
        struct {
            uint8_t sid;
            uint8_t subFunction;
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
                                                         enum Iso14229DiagnosticMode mode);
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

/**
 * @brief Poll the client request state machine.
 * @note This is not part of the user API. It is only used internally and for tests.
 *
 * @param self
 */
void Iso14229ClientPoll(Iso14229Client *self);

enum Iso14229ClientRequestError iso14229ClientSendRequest(Iso14229Client *self,
                                                          enum Iso14229DiagnosticServiceId svc,
                                                          const Iso14229RequestArgs *args);

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
