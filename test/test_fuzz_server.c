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
    uint8_t srv_retval;
    uint16_t client_sa;
    uint16_t client_ta;
    uint8_t client_func_req;
    uint8_t msg[UDS_TP_MTU];
} StuffToFuzz_t;

static StuffToFuzz_t fuzz;
static uint8_t client_recv_buf[UDS_TP_MTU];

static uint8_t fn(UDSServer_t *srv, UDSEvent_t ev, const void *arg) {
    printf("Whoah, got event %d\n", ev);
    return fuzz.srv_retval;
}

static uint32_t g_ms = 0;
uint32_t UDSMillis() { return g_ms; }
static UDSServer_t srv;
static UDSTpHandle_t *mock_client = NULL;

void DoInitialization() {
    UDSServerInit(&srv);
    srv.tp = TPMockNew("server", TPMOCK_DEFAULT_SERVER_ARGS);
    mock_client = TPMockNew("client", TPMOCK_DEFAULT_CLIENT_ARGS);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static bool initialized = false;
    if (!initialized) {
        DoInitialization();
        initialized = true;
    }
    memset(&fuzz, 0, sizeof(fuzz));
    memmove(&fuzz, data, size);

    UDSSDU_t msg = {
        .A_Mtype = UDS_A_MTYPE_DIAG,
        .A_SA = fuzz.client_sa,
        .A_TA = fuzz.client_ta,
        .A_TA_Type = fuzz.client_func_req ? UDS_A_TA_TYPE_FUNCTIONAL : UDS_A_TA_TYPE_PHYSICAL,
        .A_Length = size > offsetof(StuffToFuzz_t, msg) ? size - offsetof(StuffToFuzz_t, msg) : 0,
        .A_Data = (uint8_t *)data + offsetof(StuffToFuzz_t, msg),
        .A_DataBufSize = sizeof(fuzz.msg),
    };
    mock_client->send(mock_client, &msg);

    for (g_ms = 0; g_ms < 100; g_ms++) {
        UDSServerPoll(&srv);
    }

    {
        UDSSDU_t msg2 = {
            .A_Data = client_recv_buf,
            .A_DataBufSize = sizeof(client_recv_buf),
        };
        mock_client->recv(mock_client, &msg2);
    }
    TPMockReset();
    return 0;
}
