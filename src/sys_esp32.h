#pragma once

#if UDS_SYS == UDS_SYS_ESP32

#include <string.h>
#include <inttypes.h>
#include <esp_timer.h>

#define UDS_TP_ISOTP_C 1
#ifndef UDS_ENABLE_DBG_PRINT
#define UDS_ENABLE_DBG_PRINT 1
#endif
#define UDS_ENABLE_ASSERT 1

#endif
