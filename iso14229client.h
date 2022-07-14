#ifndef ISO14229CLIENT_H
#define ISO14229CLIENT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "iso14229.h"
#include "isotp-c/isotp.h"

#define ISO14229_CLIENT_DEFAULT_YIELD_PERIOD_MS (5U)
#define ISO14229_CLIENT_DEFAULT_P2_MS (150U)
#define ISO14229_CLIENT_DEFAULT_P2_STAR_MS (1500U)

enum Iso14229ClientRequestState {
    kRequestStateIdle = 0,          // 完成
    kRequestStateSending,           // 传输层现在传输数据
    kRequestStateSent,              // 传输层完成传输。可以设置等待计时器
    kRequestStateSentAwaitResponse, // 等待响应
    kRequestStateProcessResponse,   // 处理响应
};

enum Iso14229ClientError {
    kISO14229_SEQ_ERR_MIN = INT16_MIN, // 固定范围

    // 用户可以定义流程项管故障码在这个范围内

    kISO14229_SEQ_ERR_UNUSED = -127,
    kISO14229_SEQ_ERR_TIMEOUT,       // 流程超时
    kISO14229_SEQ_ERR_NULL_CALLBACK, // 回调函数是NULL

    kISO14229_CLIENT_ERR_RESP_SCHEMA_INVALID = -12, // 数据内容或者大小不按照应用定义(如ODX)

    kISO14229_CLIENT_ERR_RESP_DID_MISMATCH = -11,            // 响应DID对不上期待的DID
    kISO14229_CLIENT_ERR_RESP_CANNOT_UNPACK = -10,           // 响应不能解析
    kISO14229_CLIENT_ERR_RESP_TOO_SHORT = -9,                // 响应太小
    kISO14229_CLIENT_ERR_RESP_NEGATIVE = -8,                 // 否定响应
    kISO14229_CLIENT_ERR_RESP_SID_MISMATCH = -7,             // 请求和响应SID对不上
    kISO14229_CLIENT_ERR_RESP_UNEXPECTED = -6,               // 突然响应
    kISO14229_CLIENT_ERR_REQ_TIMED_OUT = -5,                 // 请求超时
    kISO14229_CLIENT_ERR_REQ_NOT_SENT_TPORT_ERR = -4,        // 传输层故障、没发
    kISO14229_CLIENT_ERR_REQ_NOT_SENT_BUF_TOO_SMALL = -3,    // 传输层缓冲器不够大
    kISO14229_CLIENT_ERR_REQ_NOT_SENT_INVALID_ARGS = -2,     // 参数不对、没发
    kISO14229_CLIENT_ERR_REQ_NOT_SENT_SEND_IN_PROGRESS = -1, // 在忙、没发
    kISO14229_CLIENT_OK = 0,                                 // 流程完成
    kISO14229_CLIENT_SEQUENCE_RUNNING = 1,                   // 流程正在跑、还没完成
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
    uint32_t p2_star_ms;
    uint16_t yield_period_ms;
    IsoTpLink *link;
    uint8_t *link_receive_buffer;
    uint16_t link_recv_buf_size;
    uint8_t *link_send_buffer;
    uint16_t link_send_buf_size;
    uint32_t (*userGetms)();
    int (*userCANTransmit)(uint32_t arb_id, const uint8_t *data, uint8_t len);
    enum Iso14229CANRxStatus (*userCANRxPoll)(uint32_t *arb_id, uint8_t *data, uint8_t *size);
    void (*userYieldms)(uint32_t duration);
    void (*userDebug)(const char *, ...);
};

enum Iso14229ClientOptions {
    SUPPRESS_POS_RESP = 0b00000001,     // 服务器不应该发送肯定响应
    FUNCTIONAL = 0b00000010,            // 发功能请求
    NEG_RESP_IS_ERR = 0b00000100,       // 否定响应是属于故障
    IGNORE_SERVER_TIMINGS = 0b00001000, // 忽略服务器给的p2和p2_star
};

typedef struct Iso14229Client {
    uint16_t phys_send_id;    // 物理发送地址
    uint16_t func_send_id;    // 功能发送地址
    uint16_t recv_id;         // 服务器相应地址
    uint16_t p2_ms;           // p2 超时时间
    uint32_t p2_star_ms;      // 0x78 p2* 超时时间
    uint16_t yield_period_ms; // 流程中等待时间
    IsoTpLink *link;
    uint32_t (*userGetms)();
    enum Iso14229CANRxStatus (*userCANRxPoll)(uint32_t *arb_id, uint8_t *data, uint8_t *size);
    void (*userYieldms)(uint32_t duration);

    // 内状态
    uint32_t p2_timer;
    struct Iso14229Request req;
    struct Iso14229Response resp;
    enum Iso14229ClientRequestState state;
    enum Iso14229ClientError err;

    enum Iso14229ClientOptions options;
    enum Iso14229ClientOptions defaultOptions;
    enum Iso14229ClientOptions _options_copy; // a copy of the options at the time a request is made
} Iso14229Client;

#define kISO14229_CLIENT_CALLBACK_DONE kISO14229_CLIENT_OK // 完成成功、可以跳到下一步函数
#define kISO14229_CLIENT_CALLBACK_PENDING                                                          \
    kISO14229_CLIENT_SEQUENCE_RUNNING // 未完成、继续调用本函数

typedef enum Iso14229ClientError (*Iso14229ClientCallback)(Iso14229Client *client, void *args);

struct Iso14229Runner;

#define ISO14229_RUNNER_STRUCT_MEMBERS                                                             \
    size_t funcIdx;                                                                                \
    const char *funcName;                                                                          \
    void (*onChange)(struct Iso14229Runner * runner);

/**
 * @brief Sequence Runner struct. Specific sequence runners should inherit from this struct
 */
struct Iso14229Runner {
    ISO14229_RUNNER_STRUCT_MEMBERS
};

struct Iso14229Sequence {
    Iso14229ClientCallback *list;
    size_t len;
};

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

void iso14229ClientInit(Iso14229Client *self, const struct Iso14229ClientConfig *cfg);
void Iso14229ClientPoll(Iso14229Client *self);

enum Iso14229ClientError ECUReset(Iso14229Client *client, enum Iso14229ECUResetResetType type);
enum Iso14229ClientError DiagnosticSessionControl(Iso14229Client *client,
                                                  enum Iso14229DiagnosticSessionType mode);
enum Iso14229ClientError SecurityAccess(Iso14229Client *client, uint8_t level, uint8_t *data,
                                        uint16_t size);
enum Iso14229ClientError CommunicationControl(Iso14229Client *client,
                                              enum Iso14229CommunicationControlType ctrl,
                                              enum Iso14229CommunicationType comm);
enum Iso14229ClientError ReadDataByIdentifier(Iso14229Client *client, const uint16_t *didList,
                                              const uint16_t numDataIdentifiers);
enum Iso14229ClientError WriteDataByIdentifier(Iso14229Client *client, uint16_t dataIdentifier,
                                               const uint8_t *data, uint16_t size);
enum Iso14229ClientError TesterPresent(Iso14229Client *client);
enum Iso14229ClientError RoutineControl(Iso14229Client *client, enum RoutineControlType type,
                                        uint16_t routineIdentifier, uint8_t *data, uint16_t size);
enum Iso14229ClientError RequestDownload(Iso14229Client *client, uint8_t dataFormatIdentifier,
                                         uint8_t addressAndLengthFormatIdentifier,
                                         size_t memoryAddress, size_t memorySize);
enum Iso14229ClientError RequestDownload_32_32(Iso14229Client *client, uint32_t memoryAddress,
                                               uint32_t memorySize);
enum Iso14229ClientError TransferData(Iso14229Client *client, uint8_t blockSequenceCounter,
                                      const uint16_t blockLength, const uint8_t *data,
                                      uint16_t size);
enum Iso14229ClientError TransferDataStream(Iso14229Client *client, uint8_t blockSequenceCounter,
                                            const uint16_t blockLength, FILE *fd);
enum Iso14229ClientError RequestTransferExit(Iso14229Client *client);
enum Iso14229ClientError ControlDTCSetting(Iso14229Client *client, uint8_t dtcSettingType,
                                           uint8_t *dtcSettingControlOptionRecord, uint16_t len);

enum Iso14229ClientError UnpackSecurityAccessResponse(Iso14229Client *client,
                                                      struct SecurityAccessResponse *resp);
enum Iso14229ClientError UnpackRoutineControlResponse(Iso14229Client *client,
                                                      struct RoutineControlResponse *resp);
enum Iso14229ClientError UnpackRequestDownloadResponse(const struct Iso14229Response *resp,
                                                       struct RequestDownloadResponse *unpacked);
int RDBIReadDID(const struct Iso14229Response *resp, uint16_t did, uint8_t *data, uint16_t size,
                uint16_t *offset);

/**
 * @brief Run a client sequence until completion or error
 * @param client
 * @param sequence
 * @param seqLen
 * @param idx a pointer to an integer to be used as the sequence index
 * @param args
 * @return int
 */
enum Iso14229ClientError iso14229SequenceRunBlocking(const struct Iso14229Sequence *seq,
                                                     Iso14229Client *client,
                                                     struct Iso14229Runner *runner);

/**
 * @brief Wait after request transmission for a response to be received
 * @note if suppressPositiveResponse is set, this function will return
 kISO14229_CLIENT_CALLBACK_DONE as soon as the transport layer has completed transmission.
 *
 * @param client
 * @param args
 * @return enum Iso14229ClientError
    kISO14229_CLIENT_OK = 0,                                 // 流程完成
    kISO14229_CLIENT_SEQUENCE_RUNNING = 1,                   // 流程正在跑、还没完成
 */
enum Iso14229ClientError Iso14229ClientAwaitIdle(Iso14229Client *client, void *args);

#endif
