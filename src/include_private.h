#pragma once
#include "include.h"

#if UDS_SYS == UDS_SYS_UNIX
#include <sys/time.h>
/* cppcheck-suppress [misra-c2012-21.10]. This is a platform-specific port. */
#include <time.h>
#endif // if UDS_SYS == UDS_SYS_UNIX

#if UDS_SYS == UDS_SYS_WINDOWS
#include <stdlib.h>
#include <time.h>
#endif // if UDS_SYS == UDS_SYS_WINDOWS

#if UDS_SYS == UDS_SYS_ARDUINO
#include <Arduino.h>
#endif // if UDS_SYS == UDS_SYS_ARDUINO

#if UDS_SYS == UDS_SYS_ESP32
#include <esp_timer.h>
#endif // if UDS_SYS == UDS_SYS_ESP32

#if UDS_SYS == UDS_SYS_ZEPHYR
#include <zephyr/kernel.h>
#endif // if UDS_SYS == UDS_SYS_ZEPHYR

#if UDS_LOG_LEVEL > UDS_LOG_NONE
#include <stdio.h>
#include <stdarg.h>
#endif // UDS_LOG_LEVEL > UDS_LOG_NONE

#ifdef UDS_TP_ISOTP_C_SOCKETCAN
#include <errno.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif // defined(UDS_TP_ISOTP_C_SOCKETCAN)

#ifdef UDS_TP_ISOTP_SOCK
#include <errno.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif // defined(UDS_TP_ISOTP_SOCK)
