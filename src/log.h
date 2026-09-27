#pragma once

/**
 * @brief logging for bring-up and unit tests.
 * Attribution: the initial version of this API was copied from ESP-IDF.
 */

#include "tp.h"
#include "config.h"
#include "includes.h"

/**
 * @defgroup uds_log_level_ valid values for UDS_LOG_LEVEL
 * @brief configures logging verbosity
 * @{
 */
#define UDS_LOG_NONE 0U    /**< No log output */
#define UDS_LOG_ERROR 1U   /**< Log errors only */
#define UDS_LOG_WARN 2U    /**< Log warnings and errors */
#define UDS_LOG_INFO 3U    /**< Log info, warnings, and errors */
#define UDS_LOG_DEBUG 4U   /**< Log debug, info, warnings, and errors */
#define UDS_LOG_VERBOSE 5U /**< Log verbose, debug, info, warnings, and errors */
/** @} */

typedef unsigned int UDS_LogLevel_t; ///< one of @ref uds_log_level_

#if UDS_LOG_LEVEL > UDS_LOG_NONE
/// \cond DOXYGEN_SHOULD_SKIP_THIS
#define UDS_LOG_FORMAT(letter, format) #letter " (%" PRIu32 ") %s: " format "\n"
#endif // UDS_LOG_LEVEL > UDS_LOG_NONE

static_assert((UDS_LOG_LEVEL == UDS_LOG_NONE) || (UDS_LOG_LEVEL == UDS_LOG_ERROR) ||
                  (UDS_LOG_LEVEL == UDS_LOG_WARN) || (UDS_LOG_LEVEL == UDS_LOG_INFO) ||
                  (UDS_LOG_LEVEL == UDS_LOG_DEBUG) || (UDS_LOG_LEVEL == UDS_LOG_VERBOSE),
              "unknown log level");

#if UDS_LOG_LEVEL >= UDS_LOG_ERROR && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGE(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_ERROR, tag, UDS_LOG_FORMAT(E, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGE(tag, format, ...) UDS_LogDummy(UDS_LOG_NONE, tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_WARN && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGW(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_WARN, tag, UDS_LOG_FORMAT(W, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGW(tag, format, ...) UDS_LogDummy(UDS_LOG_NONE, tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_INFO && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGI(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_INFO, tag, UDS_LOG_FORMAT(I, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGI(tag, format, ...) UDS_LogDummy(UDS_LOG_NONE, tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_DEBUG && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGD(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_DEBUG, tag, UDS_LOG_FORMAT(D, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGD(tag, format, ...) UDS_LogDummy(UDS_LOG_NONE, tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_VERBOSE
#define UDS_LOGV(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_VERBOSE, tag, UDS_LOG_FORMAT(V, format), UDSMillis(), tag, ##__VA_ARGS__)
#define UDS_LOG_SDU(tag, buffer, buff_len, info)                                                   \
    UDS_LogSDUInternal(UDS_LOG_DEBUG, tag, buffer, buff_len, info)
#else
#define UDS_LOGV(tag, format, ...) UDS_LogDummy(UDS_LOG_NONE, tag, format, ##__VA_ARGS__)
#define UDS_LOG_SDU(tag, buffer, buff_len, info) UDS_LogSDUDummy(UDS_LOG_NONE, tag, buffer, buff_len, info)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define UDS_PRINTF_FORMAT(fmt_index, first_arg)                                                    \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define UDS_PRINTF_FORMAT(fmt_index, first_arg)
#endif

#if UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOG_FORMAT(letter, format) #letter " (%" PRIu32 ") %s: " format "\n"
void UDS_LogWrite(UDS_LogLevel_t level, const char *tag, const char *format, ...)
    UDS_PRINTF_FORMAT(3, 4);
void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer, size_t buflen,
                        const UDSSDU_t *info);
#endif // UDS_LOG_LEVEL > UDS_LOG_NONE

static inline void UDS_LogDummy(UDS_LogLevel_t level, const char *tag, const char *format, ...) {
    (void)level;
    (void)tag;
    (void)format;
}
static inline void UDS_LogSDUDummy(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer, size_t buflen,
                                   const UDSSDU_t *info) {
    (void)level;
    (void)tag;
    (void)buffer;
    (void)buflen;
    (void)info;
}
/// \endcond
