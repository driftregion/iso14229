#pragma once

/**
 * @defgroup uds_sys_ valid values of UDS_SYS
 * @brief iso14229 host system selection
 * @see UDS_SYS
 * @{
 */
#define UDS_SYS_CUSTOM 0 /**< bare metal or unsupported targets */
#define UDS_SYS_UNIX 1
#define UDS_SYS_WINDOWS 2
#define UDS_SYS_ARDUINO 3
#define UDS_SYS_ESP32 4
/** @} */

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
#warning                                                                                           \
    "UDS_SYS was not detected, defaulting to UDS_SYS_CUSTOM. Remove this warning by defining UDS_SYS=UDS_SYS_CUSTOM in your build configuration"
#define UDS_SYS UDS_SYS_CUSTOM
#endif

#endif

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if UDS_SYS == UDS_SYS_CUSTOM
#define UDS_CUSTOM_MILLIS
#endif // UDS_SYS == UDS_SYS_CUSTOM

#if UDS_SYS == UDS_SYS_UNIX
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#endif // if UDS_SYS == UDS_SYS_UNIX

#if UDS_SYS == UDS_SYS_WINDOWS
#include <stdlib.h>
#include <time.h>
#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif // ifdef _MSC_VER
#endif // if UDS_SYS == UDS_SYS_WINDOWS

#if UDS_SYS == UDS_SYS_ARDUINO
#include <Arduino.h>
#define UDS_TP_ISOTP_C
#endif // if UDS_SYS == UDS_SYS_ARDUINO

#if UDS_SYS == UDS_SYS_ESP32
#include <esp_timer.h>
#define UDS_TP_ISOTP_C
#endif // if UDS_SYS == UDS_SYS_ESP32
