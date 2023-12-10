#pragma once

#define UDS_SYS_CUSTOM 0
#define UDS_SYS_UNIX 1
#define UDS_SYS_WINDOWS 2
#define UDS_SYS_ARDUINO 3
#define UDS_SYS_ESP32 4


#if !defined(UDS_SYS)

#if defined(__unix__) || defined(__APPLE__)
#define UDS_SYS UDS_SYS_UNIX
#elif defined(_WIN32)
#define UDS_SYS UDS_SYS_WINDOWS
#elif defined(ARDUINO)
#define UDS_SYS UDS_SYS_ARDUINO
#elif defined(ESP_PLATFORM)
#define UDS_SYS UDS_SYS_ESP32
#else
#define UDS_SYS UDS_SYS_CUSTOM
#endif

#endif

#include "sys_unix.h"
#include "sys_win32.h"
#include "sys_arduino.h"
#include "sys_esp32.h"

