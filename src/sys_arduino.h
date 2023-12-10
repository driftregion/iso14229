#pragma once

#if UDS_SYS==UDS_SYS_ARDUINO

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <Arduino.h>

#define UDS_TP UDS_TP_ISOTP_C
#define UDS_ENABLE_DBG_PRINT 1
#define UDS_ENABLE_ASSERT 1
int print_impl(const char *fmt, ...);
#define UDS_DBG_PRINT_IMPL print_impl

#endif
