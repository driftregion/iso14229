/**
 * @file main.c
 * @author Oliver Wieland (oliverwieland@web.de)
 * @brief Example DoIP client using the iso14229 library
 * @version 0.1
 * @date 2025-12-10
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "iso14229.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(UDS_TP_DOIP)

#else
#error "no transport defined"
#endif

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
        UDS_LOGE(__FILE__, "Exiting on step %d with error: %s", c->step,
                 UDSErrToStr(*(UDSErr_t *)ev_data));
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
                UDS_LOGE(__FILE__, "UDSUnpackRDBIResponse failed with err: %s",
                         UDSErrToStr(c->err));
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
    (void)ac;
    (void)av;
    UDSClient_t client;
    DoIPClient_t tp;

    // Check for invalid IP address length
    UDSErr_t result = UDSDoIPInitClient(&tp, "1234567890123456789012345678901234567890123456789012345678901234 ", 13400, 0x1234, 0x0001);
    if (result == UDS_OK) {
        UDS_LOGE(__FILE__, "DoIP Client: UDSDoIPInitClient should have failed with invalid IP address");
        UDSDoIPDeinit(&tp);
        exit(-1);
    }

    result = UDSDoIPInitClient(&tp, "127.0.0.1", 13400, 0x1234, 0x0001);
    if (result != UDS_OK) {
        UDS_LOGE(__FILE__, "DoIP Client: UDSDoIPInitClient failed with error %d", result);
        UDSDoIPDeinit(&tp);
        exit(-1);
    }

    if (UDSClientInit(&client)) {
        exit(-1);
    }

    client.tp = (UDSTp_t *)&tp;
    client.fn = fn;

    UDS_LOGI(__FILE__, "DoIP Client: UDSDoIPInitClient returned %d", result);

    SequenceContext_t ctx = {0};
    client.fn_data = &ctx;

    UDS_LOGI(__FILE__, "polling");
    while (ctx.step != Step_DONE) {
        UDSClientPoll(&client);
    }

#ifdef KEEP_ALIVE_CHECK
    // poll forver to cause keep-alive checks from server
    while (true) {
        UDSClientPoll(&client);
    }
#endif

    return ctx.err;
}
