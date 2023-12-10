#include "iso14229.h"
#include "tp_mock.h"

typedef struct {
    bool Done;
    int NumResponses;
    int Step;
} Ctx_t;

void fn(UDSClient_t *client, UDSEvent_t evt, void *evtdata, void *fndata) {
    Ctx_t *ctx = (Ctx_t *)fndata;
    switch (evt) {
    case UDS_EVT_IDLE:
        switch (ctx->Step) {
        case 0:
            client->options |= UDS_FUNCTIONAL;
            uint16_t did_list[] = {0x1, 0x8};
            UDSSendRDBI(client, did_list, sizeof(did_list) / sizeof(did_list[0]));
            ctx->Step++;
            break;
        default:
            break;
        }
        break;
    case UDS_EVT_RESP_RECV:
        switch (ctx->Step) {
        case 1:
            ctx->NumResponses++;
            // int err = UDSUnpackRDBIResponse(client, evtdata);
            ctx->Step++;
            break;
        default:
            break;
        }
        break;
    }
}

int main() {
    UDSClient_t client;
    Ctx_t ctx = {0};
    UDSServer_t server1, server2, server3;

    {
        UDSClientConfig_t cfg = {
            .tp = TPMockNew(&(TPMockCfg_t){.phys_recv_addr = 0x1,
                                           .phys_send_addr = 0x2,
                                           .func_recv_addr = 0x3,
                                           .func_send_addr = 0x4});
    }
    UDSClientInit(&client, &cfg);
}

while (!ctx.Done) {
    UDSClientPoll2(&client, fn, &ctx);
    UDSServerPoll(&server1);
    UDSServerPoll(&server2);
}
}