#pragma once

#include "sys.h"
#include "config.h"
#include "uds.h"

/**
 * @def UDS_ASSERT(x)
 * @brief used to enforce runtime invariants within iso14229
 */
#ifndef UDS_ASSERT
#define UDS_ASSERT(x) assert(x)
#endif

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
