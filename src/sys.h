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
#define UDS_SYS_ZEPHYR 5
/** @} */

#ifndef UDS_SYS   // system auto-detection
#ifdef __ZEPHYR__ // native_sim links w/host libc which also defines __unix__
#define UDS_SYS UDS_SYS_ZEPHYR
#elif defined(__unix__) || defined(__APPLE__)
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
#endif // defined(__ZEPHYR__)
#endif // #ifndef(UDS_SYS) // system auto-detection
