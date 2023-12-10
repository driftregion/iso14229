#pragma once 
#if UDS_TP == UDS_TP_ISOTP_C

#include "sys.h"
#include "config.h"
#include "uds.h"
#include "tp.h"
#include "tp/isotp-c/isotp.h"

typedef struct {
    UDSTpHandle_t hdl;
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
    int (*isotp_user_send_can)(
        const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
        void *user_data); /* send can message. should return ISOTP_RET_OK when success.  */
    uint32_t (*isotp_user_get_ms)(void);                /* get millisecond */
    void (*isotp_user_debug)(const char *message, ...); /* print debug message */
    void *user_data;                                    /* user data */
} UDSISOTpCConfig_t;

UDSErr_t UDSISOTpCInit(UDSISOTpC_t *tp, const UDSISOTpCConfig_t *cfg);

void UDSISOTpCDeinit(UDSISOTpC_t *tp);

#endif 
