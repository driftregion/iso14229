/**
 * @file test_bufferedwriter.c
 * @brief run with `gcc test_bufferedwriter.c && ./a.out`
 */

#define BUFFERED_WRITER_ASSERT assert

#include "udsserverbufferedwriter.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#ifndef BUFFEREDWRITER_DEBUG_PRINTF
#define BUFFEREDWRITER_DEBUG_PRINTF(fmt, ...) ((void)fmt)
#endif

#define LOGICAL_PARTITION_SIZE (8192 * 40)

uint8_t g_logicalPartition[LOGICAL_PARTITION_SIZE] = {0};
uint8_t g_buffer[LOGICAL_PARTITION_SIZE] = {0};

uint8_t g_mockData[LOGICAL_PARTITION_SIZE] = {0};

int writeFunc(void *addr, const uint8_t *data, uint32_t length) {
    BUFFEREDWRITER_DEBUG_PRINTF("writeFunc writing at offset %u\n",
                                addr - (void *)g_logicalPartition);
    assert((uint8_t *)addr >= g_logicalPartition);
    assert((uint8_t *)addr + length <= g_logicalPartition + sizeof(g_logicalPartition));
    memmove(addr, data, length);
    return 0;
}

static inline void print_diff(int addr) {
    int min = addr - 5 < 0 ? 0 : addr - 5;
    int max = addr + 5 > LOGICAL_PARTITION_SIZE ? LOGICAL_PARTITION_SIZE : addr + 5;
    for (int i = min; i < max; i++) {
        if (g_logicalPartition[i] != g_mockData[i]) {
            printf(">");
        } else {
            printf(" ");
        }
        printf("%d,%d,%d\n", i, g_logicalPartition[i], g_mockData[i]);
    }
}

void run_test(size_t pageBufferSize, size_t chunkSize) {
    printf("pageBufferSize:  %05x, chunkSize: %05x\n", pageBufferSize, chunkSize);
    memset(g_logicalPartition, 0, sizeof(g_logicalPartition));
    BufferedWriter bw = {0};
    BufferedWriterConfig cfg = {
        .writeFunc = writeFunc,
        .logicalPartitionStartAddr = g_logicalPartition,
        .logicalPartitionSize = sizeof(g_logicalPartition),
        .pageBufferSize = pageBufferSize,
        .pageBuffer = g_buffer,
    };

    bufferedWriterInit(&bw, &cfg);

    int n_chunks = LOGICAL_PARTITION_SIZE / chunkSize;
    size_t remainder = LOGICAL_PARTITION_SIZE % chunkSize;

    // transferData
    for (int chunk = 0; chunk <= n_chunks; chunk++) {
        bool writePending = false;
        uint32_t size = chunk == n_chunks ? remainder : chunkSize;
        do {
            writePending = BufferedWriterProcess(&bw, g_mockData + (chunk * chunkSize), size);
            if (writePending) {
                // return 0x78 and call later with the same buffer
            } else {
                // return 0x01, no writePending
            }
        } while (writePending);
    }

    // onTransferExit
    bool writePending = false;
    do {
        writePending = BufferedWriterProcess(&bw, NULL, 0);
        if (writePending) {
            // return 0x78 and call later with the same buffer
        } else {
            // return 0x01, no writePending
        }
    } while (writePending);

    if (0 != memcmp(g_logicalPartition, g_mockData, LOGICAL_PARTITION_SIZE)) {
        printf("FAIL:\n");
        printf("idx,dst,src\n");
        for (int j = 0; j < LOGICAL_PARTITION_SIZE; j++) {
            if (g_logicalPartition[j] != g_mockData[j]) {
                print_diff(j);
                break;
                // printf("%d,%d,%d\n", j, g_logicalPartition[j], g_mockData[j]);
            }
        }
        fflush(stdout);
        assert(0);
    }
}

void setup() {
    for (size_t i = 0; i < LOGICAL_PARTITION_SIZE; i++) {
        // something consistent but not too repetitive
        g_mockData[i] = (i & 0xff) + (i >> 8);
    }
}

int main(int ac, char **av) {
    setup();
    run_test(2048, 1);
    run_test(2048, 1000);
    run_test(2048, 10000);
    run_test(2048, 2047);
    run_test(2048, 2048);
    run_test(2048, 2049);
    run_test(8192, 1);
    run_test(8192, 8191);
    run_test(8192, 8192);
    run_test(8192, 8193);
    run_test(8192, 10000);
    printf("pass\n");
}