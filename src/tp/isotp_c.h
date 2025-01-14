#pragma once
#if defined(UDS_TP_ISOTP_C)

#include "sys.h"
#include "config.h"
#include "uds.h"
#include "tp.h"
#include "tp/isotp-c/isotp.h"

typedef struct {
    UDSTp_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
} UDSISOTpC_t;

typedef struct {
    uint32_t source_addr;
    uint32_t target_addr;
    uint32_t source_addr_func;
    uint32_t target_addr_func;
} UDSISOTpCConfig_t;

UDSErr_t UDSISOTpCInit(UDSISOTpC_t *tp, const UDSISOTpCConfig_t *cfg);

void UDSISOTpCDeinit(UDSISOTpC_t *tp);

#endif
