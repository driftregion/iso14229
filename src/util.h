#pragma once

#include "sys.h"
#include "config.h"
#include "uds.h"

#ifndef UDS_ASSERT
#include <assert.h>
#define UDS_ASSERT(x) assert(x)
#endif

/* returns true if `a` is after `b` */
static inline bool UDSTimeAfter(uint32_t a, uint32_t b) {
    return ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0);
}

/**
 * @brief Get time in milliseconds
 * @return current time in milliseconds
 */
uint32_t UDSMillis(void);

bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel);
bool UDSErrIsNRC(UDSErr_t err);

const char *UDSErrToStr(UDSErr_t err);
const char *UDSEventToStr(UDSEvent_t evt);
