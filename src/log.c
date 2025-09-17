#include "log.h"
#include "tp.h"
#include <stdio.h>
#include <stdarg.h>

#if UDS_LOG_LEVEL > UDS_LOG_NONE
void UDS_LogWrite(UDS_LogLevel_t level, const char *tag, const char *format, ...) {
    va_list list;
    (void)level;
    (void)tag;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}

void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer,
                        size_t buff_len, UDSSDU_t *info) {
    (void)info;
    for (unsigned i = 0; i < buff_len; i++) {
        UDS_LogWrite(level, tag, "%02x ", buffer[i]);
    }
    UDS_LogWrite(level, tag, "\n");
}
#endif
