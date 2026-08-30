#pragma once

#if UDS_SYS == UDS_SYS_RTT

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <rtt_uds_config.h>

#define UDS_TP_ISOTP_C 1
#define UDS_ENABLE_PRINTF_FORMAT_CHECK 0
#define strnlen rt_strnlen

#endif
