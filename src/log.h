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

#if CONFIG_LOG_COLORS
#define LOG_COLOR_BLACK "30"
#define LOG_COLOR_RED "31"
#define LOG_COLOR_GREEN "32"
#define LOG_COLOR_BROWN "33"
#define LOG_COLOR_BLUE "34"
#define LOG_COLOR_PURPLE "35"
#define LOG_COLOR_CYAN "36"
#define LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define LOG_RESET_COLOR "\033[0m"
#define LOG_COLOR_E LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D
#define LOG_COLOR_V
#else // CONFIG_LOG_COLORS
#define LOG_COLOR_E
#define LOG_COLOR_W
#define LOG_COLOR_I
#define LOG_COLOR_D
#define LOG_COLOR_V
#define LOG_RESET_COLOR
#endif // CONFIG_LOG_COLORS

#define LOG_FORMAT(letter, format)                                                                 \
    LOG_COLOR_##letter #letter " (%" PRIu32 ") %s: " format LOG_RESET_COLOR "\n"
#define LOG_SYSTEM_TIME_FORMAT(letter, format)                                                     \
    LOG_COLOR_##letter #letter " (%s) %s: " format LOG_RESET_COLOR "\n"

#define UDS_LOG_AT_LEVEL(level, tag, format, ...)                                                  \
    do {                                                                                           \
        if (level == UDS_LOG_ERROR) {                                                              \
            UDS_LogWrite(UDS_LOG_ERROR, tag, LOG_FORMAT(E, format), UDSMillis(), tag,              \
                         ##__VA_ARGS__);                                                           \
        } else if (level == UDS_LOG_WARN) {                                                        \
            UDS_LogWrite(UDS_LOG_WARN, tag, LOG_FORMAT(W, format), UDSMillis(), tag,               \
                         ##__VA_ARGS__);                                                           \
        } else if (level == UDS_LOG_INFO) {                                                        \
            UDS_LogWrite(UDS_LOG_INFO, tag, LOG_FORMAT(I, format), UDSMillis(), tag,               \
                         ##__VA_ARGS__);                                                           \
        } else if (level == UDS_LOG_DEBUG) {                                                       \
            UDS_LogWrite(UDS_LOG_DEBUG, tag, LOG_FORMAT(D, format), UDSMillis(), tag,              \
                         ##__VA_ARGS__);                                                           \
        } else if (level == UDS_LOG_VERBOSE) {                                                     \
            UDS_LogWrite(UDS_LOG_VERBOSE, tag, LOG_FORMAT(V, format), UDSMillis(), tag,            \
                         ##__VA_ARGS__);                                                           \
        } else {                                                                                   \
            ;                                                                                      \
        }                                                                                          \
    } while (0)

#define UDS_LOG_AT_LEVEL_LOCAL(level, tag, format, ...)                                            \
    do {                                                                                           \
        if (UDS_LOG_LEVEL >= level)                                                                \
            UDS_LOG_AT_LEVEL(level, tag, format, ##__VA_ARGS__);                                   \
    } while (0)

#define UDS_LOGE(tag, format, ...) UDS_LOG_AT_LEVEL_LOCAL(UDS_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define UDS_LOGW(tag, format, ...) UDS_LOG_AT_LEVEL_LOCAL(UDS_LOG_WARN, tag, format, ##__VA_ARGS__)
#define UDS_LOGI(tag, format, ...) UDS_LOG_AT_LEVEL_LOCAL(UDS_LOG_INFO, tag, format, ##__VA_ARGS__)
#define UDS_LOGD(tag, format, ...) UDS_LOG_AT_LEVEL_LOCAL(UDS_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#define UDS_LOGV(tag, format, ...)                                                                 \
    UDS_LOG_AT_LEVEL_LOCAL(UDS_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

#define UDS_LOG_SDU(tag, buffer, buff_len, info)                                                   \
    do {                                                                                           \
        if (UDS_LOG_LEVEL >= (UDS_LOG_DEBUG)) {                                                    \
            UDS_LogSDUInternal(UDS_LOG_DEBUG, tag, buffer, buff_len, info);                        \
        }                                                                                          \
    } while (0)


#if defined(__GNUC__) || defined(__clang__)
  #define UDS_PRINTF_FORMAT(fmt_index, first_arg) __attribute__((format(printf, fmt_index, first_arg)))
#else
  #define UDS_PRINTF_FORMAT(fmt_index, first_arg)
#endif

void UDS_LogWrite(UDS_LogLevel_t level, const char *tag, const char *format, ...)
    UDS_PRINTF_FORMAT(3, 4);
void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer,
                        size_t buff_len, UDSSDU_t *info);
