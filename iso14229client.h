#ifndef ISO14229CLIENT_H
#define ISO14229CLIENT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "iso14229.h"
#include "isotp-c/isotp.h"

typedef struct {
    uint16_t send_id;
    uint16_t recv_id;
    IsoTpLink *link;
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
    kRequestErrorResponseTooShort,       // 响应太小
    kRequestErrorCannotUnpackResponse,   // 响应不能解析
    kRequestErrorResponseTransportError, // 传输层故障、响应没有接受
};

struct Iso14229Request {
    uint8_t *buf;
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

/**
 * @brief \~chinese 客户端请求上下文 \~english User-provided callback function
 */
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

enum Iso14229ClientRequestError UnpackSecurityAccessResponse(Iso14229Client *client,
                                                             struct SecurityAccessResponse *resp);
enum Iso14229ClientRequestError UnpackRoutineControlResponse(Iso14229Client *client,
                                                             struct RoutineControlResponse *resp);
enum Iso14229ClientRequestError
UnpackRequestDownloadResponse(const struct Iso14229Response *resp,
                              struct RequestDownloadResponse *unpacked);

#define READ_DID_NO_ERR 0
#define READ_DID_ERR_RESPONSE_TOO_SHORT -1
#define READ_DID_ERR_DID_MISMATCH -2

/**
 * @brief Helper function for reading RDBI responses
 *
 * @param resp
 * @param did expected DID
 * @param data pointer to receive buffer
 * @param size number of bytes to read
 * @param offset incremented with each call
 * @return int 0 on success
 */
static inline int RDBIReadDID(const struct Iso14229Response *resp, uint16_t did, uint8_t *data,
                              uint16_t size, uint16_t *offset) {
    assert(resp);
    assert(data);
    assert(offset);
    if (0 == *offset) {
        *offset = ISO14229_0X22_RESP_BASE_LEN;
    }

    if (*offset + sizeof(did) > resp->len) {
        return READ_DID_ERR_RESPONSE_TOO_SHORT;
    }

    uint16_t theirDID = (resp->buf[*offset] << 8) + resp->buf[*offset + 1];
    if (theirDID != did) {
        return READ_DID_ERR_DID_MISMATCH;
    }

    if (*offset + sizeof(uint16_t) + size > resp->len) {
        return READ_DID_ERR_RESPONSE_TOO_SHORT;
    }

    memmove(data, &resp->buf[*offset + sizeof(uint16_t)], size);

    *offset += sizeof(uint16_t) + size;
    return READ_DID_NO_ERR;
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
