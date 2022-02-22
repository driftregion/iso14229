/**
 * @file test_bufferedwriter.c
 * @brief run with `gcc test_bufferedwriter.c && ./a.out`
 */

#include "bufferedwriter.h"
#include <assert.h>
#include <stdio.h>

#define LOGICAL_PARTITION_SIZE 0x12345

uint8_t g_logicalPartition[LOGICAL_PARTITION_SIZE] = {0};
uint8_t g_buffer[LOGICAL_PARTITION_SIZE] = {0};

uint8_t g_mockData[LOGICAL_PARTITION_SIZE] = {0};

int writeFunc(void *addr, const uint8_t *data, const size_t length) {
    BUFFEREDWRITER_DEBUG_PRINTF("writeFunc writing at offset %u\n",
                                addr - (void *)g_logicalPartition);
    memcpy(addr, data, length);
}

#define DEFAULT_BUFFERED_WRITER(forPageSize)                                                       \
    {                                                                                              \
        .writeFunc = writeFunc, .logicalPartitionStartAddr = g_logicalPartition,                   \
        .pageSize = forPageSize, .buffer = g_buffer, .currBufIdx = 0, .writeOffset = 0             \
    }

void run_test(size_t pageSize, size_t chunkSize) {
    memset(g_logicalPartition, 0, sizeof(g_logicalPartition));
    BufferedWriter bw = DEFAULT_BUFFERED_WRITER(pageSize);

    bufferedWriterInit(&bw);

    int n_chunks = LOGICAL_PARTITION_SIZE / chunkSize;
    size_t remainder = LOGICAL_PARTITION_SIZE % chunkSize;

    for (int chunk = 0; chunk < n_chunks; chunk++) {
        bufferedWriterWrite(&bw, g_mockData + (chunk * chunkSize), chunkSize);
    }

    if (remainder) {
        bufferedWriterWrite(&bw, g_mockData + (n_chunks * chunkSize), remainder);
    }

    bufferedWriterFinalize(&bw);

    if (0 != memcmp(g_logicalPartition, g_mockData, LOGICAL_PARTITION_SIZE)) {
        BUFFEREDWRITER_DEBUG_PRINTF("idx,dst,src\n");
        for (int j = 0; j < LOGICAL_PARTITION_SIZE; j++) {
            if (g_logicalPartition[j] != g_mockData[j]) {
                BUFFEREDWRITER_DEBUG_PRINTF("%d,%d,%d\n", j, g_logicalPartition[j], g_mockData[j]);
            }
        }
    }
    assert(0 == memcmp(g_logicalPartition, g_mockData, LOGICAL_PARTITION_SIZE));
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