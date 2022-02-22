#ifndef BUFFEREDWRITER_H
#define BUFFEREDWRITER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef BUFFEREDWRITER_DEBUG_PRINTF
#define BUFFEREDWRITER_DEBUG_PRINTF(fmt, ...) ((void)fmt)
#endif

typedef struct {
    /**
     * @brief writes to the program flash logical partition
     * @param address: page-aligned
     * @param len: <= pageSize
     * @param data: buffer
     * @return
     */
    int (*writeFunc)(void *address, const uint8_t *data, const size_t length);
    const void *logicalPartitionStartAddr;
    const size_t pageSize;
    const uint8_t *pageBuffer;
} BufferedWriterConfig;

typedef struct {
    const BufferedWriterConfig *cfg;
    size_t currBufIdx;
    size_t writeOffset;
} BufferedWriter;

static inline int bufferedWriterInit(BufferedWriter *self, const BufferedWriterConfig *cfg) {
    if (NULL == cfg->writeFunc || NULL == cfg->logicalPartitionStartAddr ||
        NULL == cfg->pageBuffer || 0 == cfg->pageSize) {
        return -1;
    }
    self->cfg = cfg;
    self->currBufIdx = 0;
    self->writeOffset = 0;
    return 0;
}

/**
 * @brief Always writes a full page
 *
 * @param self
 * @param data
 * @param length
 */
static inline void bufferedWriterWrite(BufferedWriter *self, uint8_t *ibuf, size_t ibufLength) {
    const BufferedWriterConfig *cfg = self->cfg;
    size_t bufferUnusedBytes, copyLen;
    size_t ibufOffset = 0;

    while (ibufLength) {
        bufferUnusedBytes = cfg->pageSize - self->currBufIdx;

        if (bufferUnusedBytes < ibufLength) {
            copyLen = bufferUnusedBytes;
        } else {
            copyLen = ibufLength;
        }

        memcpy((void *)(cfg->pageBuffer + self->currBufIdx), ibuf + ibufOffset, copyLen);
        self->currBufIdx += copyLen;
        ibufLength -= copyLen;
        ibufOffset += copyLen;

        if (cfg->pageSize == self->currBufIdx) {
            BUFFEREDWRITER_DEBUG_PRINTF("writing %u bytes at offset %u and the "
                                        "first bytes are %u, %u, %u\n",
                                        self->pageSize, self->writeOffset, self->buffer[0],
                                        self->buffer[1], self->buffer[2]);
            cfg->writeFunc((void *)(cfg->logicalPartitionStartAddr + self->writeOffset),
                           cfg->pageBuffer, cfg->pageSize);
            self->writeOffset += cfg->pageSize;
            self->currBufIdx = 0;
        }
    }
    BUFFEREDWRITER_DEBUG_PRINTF("empty: currBufIdx: %u\n", self->currBufIdx);
}

/**
 * @brief writes a partial page if necessary
 *
 * @param self
 */
static inline void bufferedWriterFinalize(BufferedWriter *self) {
    const BufferedWriterConfig *cfg = self->cfg;

    BUFFEREDWRITER_DEBUG_PRINTF("finalizing\n");
    if (self->currBufIdx) {
        BUFFEREDWRITER_DEBUG_PRINTF("f: writing %u bytes at offset %u\n", self->currBufIdx,
                                    self->writeOffset);
        cfg->writeFunc((void *)(cfg->logicalPartitionStartAddr + self->writeOffset),
                       cfg->pageBuffer, self->currBufIdx);
    }
}

#endif
