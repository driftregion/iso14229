#pragma once
#include "include.h"

#if UDS_SYS == UDS_SYS_UNIX
#include <sys/time.h>
#include <sys/types.h>
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
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#endif // defined(UDS_TP_ISOTP_C_SOCKETCAN)

#ifdef UDS_TP_ISOTP_SOCK
#include <string.h>
#include <errno.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif // defined(UDS_TP_ISOTP_SOCK)
