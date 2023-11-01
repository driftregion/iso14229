#ifndef ISOTP_C_SOCKETCAN_H
#define ISOTP_C_SOCKETCAN_H

#include "iso14229.h"
#include "tp/isotp_c.h"

typedef struct {
    UDSTpHandle_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t func_recv_buf[8];
    uint8_t func_send_buf[8];
    int fd;
} UDSTpISOTpCSocketCAN_t;

UDSTpHandle_t *UDSTpISOTpCSocketCANInit();

#endif