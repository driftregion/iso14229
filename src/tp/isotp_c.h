#pragma once
#if defined(UDS_TP_ISOTP_C)

#include "sys.h"
#include "config.h"
#include "uds.h"
#include "tp.h"
#include "tp/isotp-c/isotp.h"

/**
 * @brief isotp-c implementation of \ref UDSTp_t
 */
typedef struct {
    /// \cond DOXYGEN_SHOULD_SKIP_THIS
    UDSTp_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint8_t func_send_buf[8];
    uint8_t func_recv_buf[8];
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
    /// \endcond
} UDSTpISOTpC_t;

/**
 * @brief Initialize isotp-c transport for \ref UDSServer_t
 * @param tp \ref UDSTpISOTpC_t instance.
 * @param source_addr Server listens for physical transmissions on this address.
 * @param target_addr Server sends responses to this address.
 * @param source_addr_func Server listens for functional transmissions on this address.
 */
UDSErr_t UDSServerTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t source_addr, uint32_t target_addr,
                               uint32_t source_addr_func);

/**
 * @brief Initialize isotp-c transport for \ref UDSClient_t
 * @param tp \ref UDSTpISOTpC_t instance.
 * @param target_addr Client sends physical requests to this address.
 * @param source_addr Client listens for responses at this address.
 * @param target_addr_func Client sends functional transmissions to this address.
 */
UDSErr_t UDSClientTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t target_addr, uint32_t source_addr,
                               uint32_t target_addr_func);

#endif
