#if defined(UDS_TP_ISOTP_SOCK)

#pragma once
#include "tp.h"
#include "uds.h"

typedef struct {
    UDSTp_t hdl;
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint8_t send_buf[UDS_ISOTP_MTU];
    size_t recv_len;
    UDSSDU_t recv_info;
    int phys_fd;
    int func_fd;
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
    char tag[16];
} UDSTpIsoTpSock_t;

UDSErr_t UDSTpIsoTpSockInitServer(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func);
UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func);
void UDSTpIsoTpSockDeinit(UDSTpIsoTpSock_t *tp);

#endif
