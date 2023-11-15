#ifndef ISOTP_C_SOCKETCAN_H
#define ISOTP_C_SOCKETCAN_H

#include "iso14229.h"
#include "tp/isotp-c/isotp.h"

typedef struct {
    UDSTpHandle_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_BUFSIZE];
    uint8_t recv_buf[UDS_BUFSIZE];
    uint8_t func_recv_buf[8];
    uint8_t func_send_buf[8];
    int fd;
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
    char tag[16];
} UDSTpISOTpC_t;

UDSErr_t UDSTpISOTpCInitServer(UDSTpISOTpC_t *tp, UDSServer_t *srv, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func);
UDSErr_t UDSTpISOTpCInitClient(UDSTpISOTpC_t *tp, UDSClient_t *client, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func);
void UDSTpISOTpCDeinit(UDSTpISOTpC_t *tp);

UDSErr_t UDSTpisotpcInit(UDSTpISOTpC_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func, uint32_t target_addr_func);


#endif