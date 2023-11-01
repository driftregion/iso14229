#ifndef TP_ISOTP_C_H
#define TP_ISOTP_C_H

#include "iso14229.h"
#include "isotp-c/isotp.h"

typedef struct {
    UDSTpHandle_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t func_recv_buf[8];
    uint8_t func_send_buf[8];
} UDSTpIsoTpC_t;

#endif
