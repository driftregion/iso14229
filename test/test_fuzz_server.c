#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "iso14229.h"

#ifdef __cplusplus
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#else
int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#endif

typedef struct {
    UDSSDU_t sdu_info;
    ssize_t msg_len;
    int srv_retval;
    uint8_t msg[UDS_TP_MTU];
} StuffToFuzz_t;

static StuffToFuzz_t fuzz_buf;
static uint8_t client_recv_buf[UDS_TP_MTU];

static int fn(UDSServer_t *srv, UDSEvent_t ev, const void *arg) {
    printf("Whoah, got event %d\n", ev);
    return fuzz_buf.srv_retval;
}

static uint32_t g_ms = 0;
uint32_t UDSMillis() { return g_ms; }
static UDSServer_t srv;
static UDSTpHandle_t *mock_client = NULL;


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static bool initialized = false;
    if (!initialized) {
        UDSServerInit(&srv);
        srv.tp = TPMockNew("server", TPMOCK_DEFAULT_SERVER_ARGS);
        mock_client = TPMockNew("client", TPMOCK_DEFAULT_CLIENT_ARGS);
        initialized = true;
    }
    if (size < sizeof(fuzz_buf)) {
        return -1;
    }
    memset(&fuzz_buf, 0, sizeof(fuzz_buf));
    memmove(&fuzz_buf, data, size);

    if (fuzz_buf.msg_len > UDS_TP_MTU || fuzz_buf.msg_len < 0) {
        return -1;
    }


    // TPMockLogToStdout();
    UDSTpSend(mock_client, fuzz_buf.msg, fuzz_buf.msg_len, &fuzz_buf.sdu_info);

    for (int i = 0; i < 1000; i++) {
        UDSServerPoll(&srv);
        if(UDSTpGetRecvLen(mock_client)) {
            UDSTpAckRecv(mock_client);
        }
        g_ms++;
    }
    return 0;
}
