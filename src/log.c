#include "log.h"
#include "tp.h"
#include <stdio.h>
#include <stdarg.h>

#if UDS_SYS == UDS_SYS_RTT
#ifdef UDS_RTTHREAD_ULOG_ENABLED
#define DBG_TAG "UDS.core"
#if (UDS_LOG_LEVEL >= UDS_LOG_DEBUG)
#define DBG_LVL LOG_LVL_DBG
#elif (UDS_LOG_LEVEL == UDS_LOG_INFO)
#define DBG_LVL LOG_LVL_INFO
#elif (UDS_LOG_LEVEL == UDS_LOG_WARN)
#define DBG_LVL LOG_LVL_WARNING
#elif (UDS_LOG_LEVEL == UDS_LOG_ERROR)
#define DBG_LVL LOG_LVL_ERROR
#else
#define DBG_LVL LOG_LVL_ASSERT
#endif
#include <rtdbg.h>
#endif
#endif

#if UDS_LOG_LEVEL > UDS_LOG_NONE
void UDS_LogWrite(UDS_LogLevel_t level, const char *tag, const char *format, ...) {
    va_list list;
    (void)level;
    (void)tag;
    va_start(list, format);
#if UDS_SYS == UDS_SYS_RTT
#ifdef UDS_RTTHREAD_ULOG_ENABLED
    ulog_voutput(DBG_LVL, DBG_TAG, RT_TRUE, RT_NULL, 0, 0, 0, format, list);
#else
    char log_buf[UDS_RTTHREAD_LOG_BUFFER_SIZE];
    rt_vsnprintf(log_buf, sizeof(log_buf), format, list);
    rt_kprintf("%s", log_buf);
#endif
#else
    vprintf(format, list);
#endif
    va_end(list);
}

void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer,
                        size_t buff_len, UDSSDU_t *info) {
    (void)info;
#if UDS_SYS == UDS_SYS_RTT && defined(UDS_RTTHREAD_ULOG_ENABLED)
    ulog_hexdump(tag, 16, (rt_uint8_t *)buffer, buff_len);
#else
    for (unsigned i = 0; i < buff_len; i++) {
        UDS_LogWrite(level, tag, "%02x ", buffer[i]);
    }
    UDS_LogWrite(level, tag, "\n");
#endif
}
#endif
