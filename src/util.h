#pragma once

#include "sys.h"
#include "config.h"
#include "uds.h"

#ifndef UDS_ASSERT
#define UDS_ASSERT(x) assert(x)
#endif

/**
 * @brief Check whether one timestamp is after another, correctly handling wrap-around
 * @param a: timestamp to check
 * @param b: reference timestamp
 * @return true if `a` is after `b`
 */
static inline bool UDSTimeAfter(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

/**
 * @brief Get time in milliseconds
 * @return current time in milliseconds
 * @note implementers must ensure the return value is monotonically increasing between
 * calls. The value must never go backwards.
 * Wrap-around (overflow back to 0) is expected; this is handled by UDSTimeAfter.
 */
uint32_t UDSMillis(void);

const char *UDSErrToStr(UDSErr_t err);
const char *UDSEventToStr(UDSEvent_t evt);
