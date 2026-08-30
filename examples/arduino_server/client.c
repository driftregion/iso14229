/**
 * @file examples/arduino_server/client.c
 * @brief UDS client to exercise the Arduino UDS server
 */
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
        Step_1_ECUReset_Send,
        Step_2_ECUReset_Recv,
        Step_DONE,
    } step;
    UDSErr_t err;
} SequenceContext_t;

UDSErr_t fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    SequenceContext_t *c = (SequenceContext_t *)client->fn_data;
    if (evt != UDS_EVT_Poll) {
        UDS_LOGI(__FILE__, "%s (%d)", UDSEventToStr(evt), evt);
    }
    if (UDS_EVT_Err == evt) {
        UDS_LOGE(__FILE__, "Exiting on step %d with error: %s", c->step,
                 UDSErrToStr(*(UDSErr_t *)ev_data));
        c->err = *(UDSErr_t *)ev_data;
        c->step = Step_DONE;
    }
    switch (c->step) {
    case Step_1_ECUReset_Send: {

        c->err = UDSSendECUReset(client, UDS_LEV_RT_HR);
        if (c->err) {
            UDS_LOGE(__FILE__, "UDSSendECUReset failed with err: %s", UDSErrToStr(c->err));
            c->step = Step_DONE;
        }
        c->step = Step_2_ECUReset_Recv;
        break;
    }
    case Step_2_ECUReset_Recv: {
        if (UDS_EVT_ResponseReceived == evt) {
            UDS_LOGI(__FILE__, "ECUReset response received");
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

    if (UDSClientTpIsoTpSockInit(&tp, "can0", 0x7E8, 0x7E0, 0x7DF)) {
        UDS_LOGE(__FILE__, "UDSClientTpIsoTpSockInit failed");
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
