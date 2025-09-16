#pragma once

/**
 * @brief logging for bring-up and unit tests.
 * This interface was copied from ESP-IDF.
 */

#include "sys.h"
#include "config.h"
#include "uds.h"
#include "tp.h"

typedef enum {
    UDS_LOG_NONE,    // No log output
    UDS_LOG_ERROR,   // Log errors only
    UDS_LOG_WARN,    // Log warnings and errors
    UDS_LOG_INFO,    // Log info, warnings, and errors
    UDS_LOG_DEBUG,   // Log debug, info, warnings, and errors
    UDS_LOG_VERBOSE, // Log verbose, debug, info, warnings, and errors
} UDS_LogLevel_t;

#ifndef UDS_LOG_LEVEL
#define UDS_LOG_LEVEL UDS_LOG_NONE
#endif

#if UDS_CONFIG_LOG_COLORS
#define UDS_LOG_COLOR_BLACK "30"
#define UDS_LOG_COLOR_RED "31"
#define UDS_LOG_COLOR_GREEN "32"
#define UDS_LOG_COLOR_BROWN "33"
#define UDS_LOG_COLOR_BLUE "34"
#define UDS_LOG_COLOR_PURPLE "35"
#define UDS_LOG_COLOR_CYAN "36"
#define LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define UDS_LOG_RESET_COLOR "\033[0m"
#define UDS_LOG_COLOR_E LOG_COLOR(UDS_LOG_COLOR_RED)
#define UDS_LOG_COLOR_W LOG_COLOR(UDS_LOG_COLOR_BROWN)
#define UDS_LOG_COLOR_I LOG_COLOR(UDS_LOG_COLOR_GREEN)
#define UDS_LOG_COLOR_D
#define UDS_LOG_COLOR_V
#else // UDS_CONFIG_LOG_COLORS
#define UDS_LOG_COLOR_E
#define UDS_LOG_COLOR_W
#define UDS_LOG_COLOR_I
#define UDS_LOG_COLOR_D
#define UDS_LOG_COLOR_V
#define UDS_LOG_RESET_COLOR
#endif // UDS_CONFIG_LOG_COLORS

#define UDS_LOG_FORMAT(letter, format)                                                             \
    UDS_LOG_COLOR_##letter #letter " (%" PRIu32 ") %s: " format UDS_LOG_RESET_COLOR "\n"

#if UDS_LOG_LEVEL >= UDS_LOG_ERROR && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGE(tag, format, ...) \
    UDS_LogWrite(UDS_LOG_ERROR, tag, UDS_LOG_FORMAT(E, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGE(tag, format, ...) ((void)0)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_WARN && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGW(tag, format, ...) \
    UDS_LogWrite(UDS_LOG_WARN, tag, UDS_LOG_FORMAT(W, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGW(tag, format, ...) ((void)0)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_INFO && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGI(tag, format, ...) \
    UDS_LogWrite(UDS_LOG_INFO, tag, UDS_LOG_FORMAT(I, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGI(tag, format, ...) ((void)0)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_DEBUG && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGD(tag, format, ...) \
    UDS_LogWrite(UDS_LOG_DEBUG, tag, UDS_LOG_FORMAT(D, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGD(tag, format, ...) ((void)0)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_VERBOSE && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGV(tag, format, ...) \
    UDS_LogWrite(UDS_LOG_VERBOSE, tag, UDS_LOG_FORMAT(V, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGV(tag, format, ...) ((void)0)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_DEBUG && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOG_SDU(tag, buffer, buff_len, info) \
    UDS_LogSDUInternal(UDS_LOG_DEBUG, tag, buffer, buff_len, info)
#else
#define UDS_LOG_SDU(tag, buffer, buff_len, info) ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define UDS_PRINTF_FORMAT(fmt_index, first_arg)                                                    \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define UDS_PRINTF_FORMAT(fmt_index, first_arg)
#endif

#if UDS_LOG_LEVEL > UDS_LOG_NONE
void UDS_LogWrite(UDS_LogLevel_t level, const char *tag, const char *format, ...)
    UDS_PRINTF_FORMAT(3, 4);
void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer,
                        size_t buff_len, UDSSDU_t *info);
#endif
