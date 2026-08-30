#if defined(UDS_TP_ISOTP_SOCK)

#pragma once
#include "tp.h"
#include "uds.h"

/**
 * @brief linux ISO-TP socket implementation of \ref UDSTp_t
 */
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
                                  uint32_t target_addr,
                                  uint32_t source_addr_func); ///< for UDSServer_t
UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr,
                                  uint32_t target_addr_func); ///< for UDSClient_t
void UDSTpIsoTpSockDeinit(UDSTpIsoTpSock_t *tp);              ///< release sockets

#endif
