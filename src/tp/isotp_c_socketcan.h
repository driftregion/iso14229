#pragma once

#if defined(UDS_TP_ISOTP_C_SOCKETCAN)

#include "tp.h"
#include "tp/isotp-c/isotp.h"

typedef struct {
    UDSTp_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    int fd;
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
    char tag[16];
} UDSTpISOTpC_t;

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, const char *ifname, uint32_t source_addr,
                         uint32_t target_addr, uint32_t source_addr_func,
                         uint32_t target_addr_func);
void UDSTpISOTpCDeinit(UDSTpISOTpC_t *tp);

#endif
