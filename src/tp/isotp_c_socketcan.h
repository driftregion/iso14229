#pragma once

#if defined(UDS_TP_ISOTP_C_SOCKETCAN)

#include "tp.h"
#include "tp/isotp-c/isotp.h"

/**
 * @brief isotp-c over SocketCAN implementation of \ref UDSTp_t
 */
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
} UDSTpISOTpCSocketCAN_t;

/**
 * @brief Initialize the transport
 * @param tp transport
 * @param ifname can0, vcan0
 * @param source_addr
 * @param target_addr
 * @param source_addr_func
 * @param target_addr_func
 */
UDSErr_t UDSTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                  uint32_t source_addr, uint32_t target_addr,
                                  uint32_t source_addr_func, uint32_t target_addr_func);
void UDSTpISOTpCSocketCANDeinit(UDSTpISOTpCSocketCAN_t *tp); ///< release socket

#endif
