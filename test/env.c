#include "test/env.h"
#include "iso14229.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

UDSTpHandle_t *Handles[8] = {0};
unsigned HandleCount = 0;

UDSTpHandle_t *ENV_TpNew() {
    if (HandleCount >= sizeof(Handles) / sizeof(Handles[0])) {
        fprintf(stderr, "Too many TP handles\n");
        return NULL;
    }
    UDSTpHandle_t *tp = malloc(sizeof(UDSTpHandle_t));
    memset(tp, 0, sizeof(UDSTpHandle_t));
    Handles[HandleCount++] = tp;
    return tp;
}

void ENV_Send(UDSSDU_t *msg) {
    assert(msg);
    for (unsigned i = 0; i < HandleCount; i++) {
        UDSTpHandle_t *tp = Handles[i];
        if (tp == NULL) {
            continue;
        }
        if (tp->)
        UDSTpSend(tp, msg);
    }
    UDSTpHandle_t *tp = msg->A_TP;
    if (tp == NULL) {
        tp = ENV_TpNew();
        msg->A_TP = tp;
    }
    UDSTpSend(tp, msg);
}