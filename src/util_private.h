#pragma once

#include "sys.h"

/*
 * Serializes n bytes of val to *dst in big-endian format.
 */
static inline void StoreBE(uint8_t *dst, uint64_t val, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = (uint8_t)(val >> (8 * (n - 1 - i)));
    }
}

/*
 * Mirror operation
 */
static inline uint64_t LoadBE(const uint8_t *src, size_t n) {
    uint64_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    return val;
}
