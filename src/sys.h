#pragma once

#define UDS_SYS_CUSTOM 0
#define UDS_SYS_UNIX 1
#define UDS_SYS_WINDOWS 2
#define UDS_SYS_ARDUINO 3
#define UDS_SYS_ESP32 4

#if !defined(UDS_SYS)

#if defined(__unix__) || defined(__APPLE__)
#define UDS_SYS UDS_SYS_UNIX
#include "sys_unix.h"
#elif defined(_WIN32)
#define UDS_SYS UDS_SYS_WINDOWS
#include "sys_win32.h"
#elif defined(ARDUINO)
#define UDS_SYS UDS_SYS_ARDUINO
#include "sys_arduino.h"
#elif defined(ESP_PLATFORM)
#define UDS_SYS UDS_SYS_ESP32
#include "sys_esp32.h"
#else
#define UDS_SYS UDS_SYS_CUSTOM
#include "uds_sys_custom.h"
#endif

#endif
