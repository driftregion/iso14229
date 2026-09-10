#pragma once

#if defined(UDS_TP_ISOTP_C_SOCKETCAN)

#include "tp.h"
#include "tp/isotp_c.h"

/**
 * @brief isotp-c over SocketCAN implementation of \ref UDSTp_t
 */
typedef struct {
    /// \cond DOXYGEN_SHOULD_SKIP_THIS
    UDSTpISOTpC_t hdl2;
    int fd;
    char tag[16];
    /// \endcond
} UDSTpISOTpCSocketCAN_t;

/**
 * @brief Initialize isotp-c over SocketCAN transport for \ref UDSServer_t
 * @param tp \ref UDSTpISOTpSocketCAN_t instance.
 * @param source_addr Server listens for physical transmissions on this address.
 * @param target_addr Server sends responses to this address.
 * @param source_addr_func Server listens for functional transmissions on this address.
 */
UDSErr_t UDSServerTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                        uint32_t source_addr, uint32_t target_addr,
                                        uint32_t source_addr_func);

/**
 * @brief Initialize isotp-c over SocketCAN transport for \ref UDSClient_t
 * @param tp \ref UDSTpISOTpC_t instance.
 * @param source_addr Client listens for responses at this address.
 * @param target_addr Client sends physical requests to this address.
 * @param target_addr_func Client sends functional transmissions to this address.
 */
UDSErr_t UDSClientTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                        uint32_t source_addr, uint32_t target_addr,
                                        uint32_t target_addr_func);

void UDSTpISOTpCSocketCANDeinit(UDSTpISOTpCSocketCAN_t *tp); ///< release socket

#endif
