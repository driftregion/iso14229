#pragma once

#include "sys.h"
#include "uds.h"

/// Serializes n bytes of val to *dst in big-endian format.
static inline void PackBE(uint8_t *dst, uint64_t val, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = (uint8_t)(val >> (8u * (n - 1u - i)));
    }
}

/**
 * @brief Unpack up to sizeof(size_t) big-endian bytes from src into dst.
 * @param src buffer
 * @param dst
 * @param n ranges from 0 to sizeof(size_t) inclusive
 * @return UDS_OK if successful
 */
static inline UDSErr_t UnpackBEsize(const uint8_t *src, size_t *dst, size_t n) {
    if (NULL == src || NULL == dst || n > sizeof(*dst)) {
        return UDS_ERR_INVALID_ARG;
    }
    size_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    *dst = val;
    return UDS_OK;
}

/**
 * @brief Unpack up to sizeof(uintptr_t) big-endian bytes from src into dst.
 * @param src buffer
 * @param dst
 * @param n ranges from 0 to sizeof(uintptr_t) inclusive
 * @return UDS_OK if successful
 */
static inline UDSErr_t UnpackBEuintptr(const uint8_t *src, uintptr_t *dst, size_t n) {
    if (NULL == src || NULL == dst || n > sizeof(*dst)) {
        return UDS_ERR_INVALID_ARG;
    }
    uintptr_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    *dst = val;
    return UDS_OK;
}

/**
 * @brief Unpack up to 4 big-endian bytes from src into dst as uint32_t.
 * @param src buffer
 * @param dst pointer to destination
 * @param n ranges from 0 to 4 inclusive
 * @return UDS_OK if successful
 */
static inline UDSErr_t UnpackBEu32(const uint8_t *src, uint32_t *dst, size_t n) {
    if (NULL == src || NULL == dst || n > sizeof(*dst)) {
        return UDS_ERR_INVALID_ARG;
    }
    uint32_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8) | src[i];
    }
    *dst = val;
    return UDS_OK;
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
    UDS_ASSERT(request_sid <= UINT8_MAX - 0x40u);
    return request_sid + 0x40u;
}

static inline uint8_t AsRequestSID(uint8_t response_sid) {
    UDS_ASSERT(response_sid >= 0x40u);
    return response_sid - 0x40u;
}

/// returns true if a security level is reserved per ISO14229-1:2020 Table 42
bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel);

/// returns true if err is defined in ISO14229-1:2020 as an NRC
bool UDSErrIsNRC(UDSErr_t err);
