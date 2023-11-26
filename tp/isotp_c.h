#ifndef ISOTP_C_H
#define ISOTP_C_H

#include "iso14229.h"
#include "tp/isotp-c/isotp.h"

typedef struct {
    UDSTpHandle_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
} UDSTpISOTpC_t;

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
} UDSTpISOTpCConfig_t;

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, UDSTpISOTpCConfig_t *cfg);

void UDSTpISOTpCDeinit(UDSTpISOTpC_t *tp);

#endif
