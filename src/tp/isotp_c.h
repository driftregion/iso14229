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
    UDSTp_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint8_t func_send_buf[8];
    uint8_t func_recv_buf[8];
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
} UDSTpISOTpC_t;

/**
 * @brief arguments for \ref UDSTpISOTpCInit
 */
typedef struct {
    uint32_t source_addr;
    uint32_t target_addr;
    uint32_t source_addr_func;
    uint32_t target_addr_func;
} UDSTpISOTpCConfig_t;

/**
 * @brief Initialize isotp-c transport
 */
UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, const UDSTpISOTpCConfig_t *cfg);

#endif
