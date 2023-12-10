/**
 * @file udsserverbufferedwriter.h
 *
 * [ UDS接收缓冲器 ] -BufferedWriterWrite()-> [FLASH扇区缓冲器] -writeFunc()-> [FLASH]
 *
 * - 写到FLASH时、要先发一个0x78响应然后去写
 */

#ifndef BUFFEREDWRITER_H
#define BUFFEREDWRITER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

typedef struct {
    /**
     * @brief writes to the program flash logical partition
     * @param address: page-aligned
     * @param len: <= pageBufferSize
     * @param data: buffer
     * @return
     */
    int (*writeFunc)(void *address, const uint8_t *data, const uint32_t length);
    const void *logicalPartitionStartAddr;
    const uint32_t logicalPartitionSize;
    const uint8_t *pageBuffer;     // pointer to a buffer that is the size of a flash page
    const uint32_t pageBufferSize; // size of a flash page in bytes
} BufferedWriterConfig;

typedef struct {
    const BufferedWriterConfig *cfg;
    uint32_t pageBufIdx;
    uint32_t flashOffset;
    uint32_t iBufIdx;
    bool writePending;
} BufferedWriter;

static inline void bufferedWriterInit(BufferedWriter *self, const BufferedWriterConfig *cfg) {
    assert(cfg->writeFunc);
    assert(cfg->logicalPartitionStartAddr);
    assert(cfg->pageBuffer);
    assert(cfg->pageBufferSize > 0);
    assert(cfg->logicalPartitionSize >= cfg->pageBufferSize);
    assert(cfg->logicalPartitionSize % cfg->pageBufferSize == 0);

    self->writePending = false;
    self->cfg = cfg;
    self->iBufIdx = 0;
    self->pageBufIdx = 0;
    self->flashOffset = 0;
    memset((void *)cfg->pageBuffer, 0, cfg->pageBufferSize);
}

static inline void _BufferedWriterCopy(BufferedWriter *self, const uint8_t *ibuf, uint32_t size) {
    if (0 == size) {
        return;
    }
    const BufferedWriterConfig *cfg = self->cfg;
    uint32_t bufferUnusedBytes = cfg->pageBufferSize - self->pageBufIdx;
    uint32_t copyLen =
        size - self->iBufIdx < bufferUnusedBytes ? size - self->iBufIdx : bufferUnusedBytes;
    memmove((void *)(cfg->pageBuffer + self->pageBufIdx), ibuf + self->iBufIdx, copyLen);
    self->pageBufIdx += copyLen;
    self->iBufIdx += copyLen;
}

static inline void _BufferedWriterWrite(BufferedWriter *self) {
    const BufferedWriterConfig *cfg = self->cfg;
    cfg->writeFunc((void *)((uint8_t *)cfg->logicalPartitionStartAddr + self->flashOffset),
                   cfg->pageBuffer, cfg->pageBufferSize);
    self->flashOffset += cfg->pageBufferSize;
    self->pageBufIdx = 0;
    memset((void *)cfg->pageBuffer, 0, cfg->pageBufferSize);
}

/**
 * @brief 处理flash写入、包含0x78 RequestCorrectlyReceived_ResponsePending响应。
 * 为了了解更多关于0x78的作用、可以参考ISO-14229-1:2013。
 *
 * @param self 指针到驱动实例
 * @param ibuf 输入缓冲器 (UDS接收缓冲器)
 * @param size 输入缓冲器大小
 * @return true 等待写入。你应该返回0x78。不要换输入缓冲器。我还没读完。
 * @return false 不在等、可以返回0x01。
 */
static inline bool BufferedWriterProcess(BufferedWriter *self, const uint8_t *ibuf, uint32_t size) {

    if (self->writePending) { // 要写入
        _BufferedWriterWrite(self);
        if (self->iBufIdx == size) {
            // 输入缓冲器用完了、我要新数据
            self->iBufIdx = 0;
            return self->writePending = false;
        }
    }

    _BufferedWriterCopy(self, ibuf, size);

    if (
        // 扇区缓冲器充满了、要写入
        (self->pageBufIdx == self->cfg->pageBufferSize) ||
        // 扇区缓冲器有数据、用户要完成写入
        (0 != self->pageBufIdx && 0 == size)) {
        return self->writePending = true;
    } else {
        self->iBufIdx = 0;
        return self->writePending = false;
    }
}

#endif
