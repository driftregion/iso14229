#pragma once

#include "sys.h"
#include "uds.h"

/// Serializes n bytes of val to *dst in big-endian format.
static inline void PackBE(uint8_t *dst, uint64_t val, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = (uint8_t)(val >> (8U * (n - 1U - i)));
    }
}

/**
 * @brief Unpack up to sizeof(size_t) big-endian bytes from src.
 * @param src buffer
 * @param n ranges from 0 to sizeof(size_t) inclusive
 * @return unpacked quantity
 */
static inline size_t UnpackBEsize(const uint8_t *src, size_t n) {
    UDS_ASSERT(src != NULL);
    UDS_ASSERT(n <= sizeof(size_t));

    size_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    return val;
}

/**
 * @brief Unpack up to sizeof(uintptr_t) big-endian bytes from src.
 * @param src buffer
 * @param n ranges from 0 to sizeof(uintptr_t) inclusive
 * @return unpacked quantity
 */
static inline uintptr_t UnpackBEuintptr(const uint8_t *src, size_t n) {
    UDS_ASSERT(src != NULL);
    UDS_ASSERT(n <= sizeof(uintptr_t));

    uintptr_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    return val;
}

/**
 * @brief Unpack up to 4 big-endian bytes from src into dst as uint32_t.
 * @param src buffer
 * @param n ranges from 0 to 4 inclusive
 * @return unpacked quantity
 */
static inline uint32_t UnpackBEu32(const uint8_t *src, size_t n) {
    UDS_ASSERT(src != NULL);
    UDS_ASSERT(n <= sizeof(uint32_t));

    uint32_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }

    return val;
}

/**
 * @brief Unpack 2 big-endian bytes from src into dst as uint16_t.
 * @param src buffer
 * @param dst pointer to destination
 * @return UDS_OK if successful
 */
static inline uint16_t UnpackBEu16(const uint8_t *src) {
    UDS_ASSERT(src);
    return (uint16_t)((uint16_t)(src[0] << 8) | (uint16_t)src[1]);
}

static inline uint8_t AsResponseSID(uint8_t request_sid) {
    UDS_ASSERT(request_sid <= (UINT8_MAX - 0x40U));
    return request_sid + 0x40U;
}

static inline uint8_t AsRequestSID(uint8_t response_sid) {
    UDS_ASSERT(response_sid >= 0x40U);
    return response_sid - 0x40U;
}

/// returns true if a security level is reserved per ISO14229-1:2020 Table 42
bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel);

/// returns true if err is defined in ISO14229-1:2020 as an NRC
bool UDSErrIsNRC(UDSErr_t err);

/**
 * @brief Check whether one timestamp is after another, correctly handling wrap-around
 * @param a: timestamp to check
 * @param b: reference timestamp
 * @return true if `a` is after `b`
 */
static inline bool UDSTimeAfter(uint32_t a, uint32_t b) {
    uint32_t diff = a - b;
    return (diff != 0U) && ((diff & 0x80000000U) == 0U);
}
