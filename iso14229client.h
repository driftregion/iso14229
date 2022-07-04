#ifndef ISO14229CLIENT_H
#define ISO14229CLIENT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "iso14229.h"
#include "isotp-c/isotp.h"

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

struct Iso14229ClientConfig {
    uint16_t phys_send_id;
    uint16_t func_send_id;
    uint16_t recv_id;
    uint16_t p2_ms;
    uint16_t p2_star_ms;
    IsoTpLink *link;
    uint8_t *link_receive_buffer;
    uint16_t link_recv_buf_size;
    uint8_t *link_send_buffer;
    uint16_t link_send_buf_size;
    uint32_t (*userGetms)();
    int (*userCANTransmit)(uint32_t arb_id, const uint8_t *data, uint8_t len);
    enum Iso14229CANRxStatus (*userCANRxPoll)(uint32_t *arb_id, uint8_t *data, uint8_t *size);
    void (*userSleepms)();
    void (*userDebug)(const char *, ...);
};

typedef struct Iso14229Client {
    uint16_t phys_send_id; // 物理发送地址
    uint16_t func_send_id; // 功能发送地址
    uint16_t recv_id;      // 服务器相应地址
    uint16_t p2_ms;        // p2 超时时间
    uint16_t p2_star_ms;   // 0x78 p2* 超时时间
    IsoTpLink *link;
    uint32_t (*userGetms)();
    enum Iso14229CANRxStatus (*userCANRxPoll)(uint32_t *arb_id, uint8_t *data, uint8_t *size);
    void (*userSleepms)();

    // 内状态
    uint32_t p2_timer;
    struct Iso14229Request req;
    struct Iso14229Response resp;
    enum Iso14229ClientRequestState state;
    enum Iso14229ClientRequestError err;

    // 客户端配置 / client options
    struct {
        // 服务器不应该发送肯定响应
        uint8_t suppressPositiveResponse : 1;
        // 发功能请求
        uint8_t sendFunctional : 1;
        // 否定响应是否属于故障
        uint8_t negativeResponseIsError : 1;
        // 预留
        uint8_t reserved : 5;
    };
} Iso14229Client;

enum Iso14229ClientCallbackStatus {
    kISO14229_CLIENT_CALLBACK_ERROR = -1,  // 故障、立刻停止
    kISO14229_CLIENT_CALLBACK_PENDING = 0, // 未完成、继续调用本函数
    kISO14229_CLIENT_CALLBACK_DONE = 1,    // 完成成功、可以跳到下一步函数
};

typedef enum Iso14229ClientCallbackStatus (*Iso14229ClientCallback)(Iso14229Client *client,
                                                                    void *args);

struct Iso14229ClientStep {
    Iso14229ClientCallback cb;
    void *args;
};

/**
 * @brief Run a sequence until completion or error
 * @param client
 * @param sequence
 * @param seqLen
 * @param idx a pointer to an integer to be used as the sequence index
 * @param args
 * @return int
 */
int iso14229ClientSequenceRunBlocking(Iso14229Client *client, struct Iso14229ClientStep sequence[],
                                      size_t seqLen, int *idx);

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
void iso14229ClientInit(Iso14229Client *self, const struct Iso14229ClientConfig *cfg);

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
                                             const uint16_t blockLength, const uint8_t *data,
                                             uint16_t size);
enum Iso14229ClientRequestError TransferDataStream(Iso14229Client *client,
                                                   uint8_t blockSequenceCounter,
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
 * @param self
 */
void Iso14229ClientPoll(Iso14229Client *self);

#endif
