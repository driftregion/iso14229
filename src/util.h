#pragma once

#include "sys.h"
#include "config.h"
#include "uds.h"

#if UDS_ENABLE_ASSERT
#include <assert.h>
#define UDS_ASSERT(x) assert(x)
#else
#define UDS_ASSERT(x)
#endif

#if UDS_ENABLE_DBG_PRINT
#if defined(UDS_DBG_PRINT_IMPL)
#define UDS_DBG_PRINT UDS_DBG_PRINT_IMPL
#else
#include <stdio.h>
#define UDS_DBG_PRINT printf
#endif
#else
#define UDS_DBG_PRINT(fmt, ...) ((void)fmt)
#endif

#define UDS_DBG_PRINTHEX(addr, len)                                                                \
    for (int i = 0; i < len; i++) {                                                                \
        UDS_DBG_PRINT("%02x,", ((uint8_t *)addr)[i]);                                              \
    }                                                                                              \
    UDS_DBG_PRINT("\n");

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

const char *UDSErrToStr(UDSErr_t err);
const char *UDSEvtToStr(UDSEvent_t evt);
