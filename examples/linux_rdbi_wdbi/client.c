#include "iso14229.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

typedef struct {
    enum {
        Step_0_RDBI_Send,
        Step_1_RDBI_Recv,
        Step_2_WDBI_Send,
        Step_3_WDBI_Recv,
        Step_DONE,
    } step;
    UDSErr_t err;
    uint16_t rdbi_f190;
} SequenceContext_t;


UDSErr_t fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    SequenceContext_t *c = (SequenceContext_t *)client->fn_data;
    if (evt != UDS_EVT_Poll) {
        UDS_LOGI(__FILE__, "%s (%d)", UDSEventToStr(evt), evt);
    }
    if (UDS_EVT_Err == evt) {
        UDS_LOGE(__FILE__, "Exiting on step %d with error: %s", c->step, UDSErrToStr(*(UDSErr_t*)ev_data));
        c->err = *(UDSErr_t *)ev_data;
        c->step = Step_DONE;
    }
    switch (c->step) {
        case Step_0_RDBI_Send: {
            const uint16_t dids[] = {0xf190};
            c->err = UDSSendRDBI(client, dids, 1);
            if (c->err) {
                UDS_LOGE(__FILE__, "UDSSendRDBI failed with err: %d", c->err);
                c->step = Step_DONE;
            }
            c->step = Step_1_RDBI_Recv;
            break;
        }
        case Step_1_RDBI_Recv: {
            UDSRDBIVar_t vars[] = {
                {0xf190, 2, &(c->rdbi_f190), memmove},
            };
            if (UDS_EVT_ResponseReceived == evt) {
                c->err = UDSUnpackRDBIResponse(client, vars, 1);
                if (c->err) {
                    UDS_LOGE(__FILE__, "UDSUnpackRDBIResponse failed with err: %s", UDSErrToStr(c->err));
                    c->step = Step_DONE;
                }
                UDS_LOGI(__FILE__, "0xf190 has value %d", c->rdbi_f190);
                c->step = Step_2_WDBI_Send;
            }
            break;
            }
        case Step_2_WDBI_Send: {
            uint16_t val = c->rdbi_f190 + 1;
            uint8_t data[2] = {
                (val & 0xff00) >> 8,
                val & 0x00ff,
            };
            c->err = UDSSendWDBI(client, 0xf190, data, sizeof(data));
            if (c->err) {
                UDS_LOGE(__FILE__, "UDSSendWDBI failed with err: %s", UDSErrToStr(c->err));
                c->step = Step_DONE;
            }
            c->step = Step_3_WDBI_Recv;
            break;
        }
        case Step_3_WDBI_Recv: {
            if (UDS_EVT_ResponseReceived == evt) {
                UDS_LOGI(__FILE__, "WDBI response received");
                c->step = Step_DONE;
            }
        default:
            break;
        }
    }
    return UDS_OK;
}

int main(int ac, char **av) {
    UDSClient_t client;
    UDSTpIsoTpSock_t tp;

    if (UDSTpIsoTpSockInitClient(&tp, "vcan0", 0x7E8, 0x7E0, 0x7DF)) {
        UDS_LOGE(__FILE__, "UDSTpIsoTpSockInitClient failed");
        exit(-1);
    }

    if (UDSClientInit(&client)) {
        exit(-1);
    }

    client.tp = (UDSTp_t *)&tp;
    client.fn = fn;

    SequenceContext_t ctx = {0};
    client.fn_data = &ctx;

    UDS_LOGI(__FILE__, "polling");
    while (ctx.step != Step_DONE) {
        UDSClientPoll(&client);
    }

    return ctx.err;
}
