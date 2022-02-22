#include "simple.h"
#include "../iso14229.h"
#include <stdint.h>

#define SRV_PHYS_RECV_ID 0x7A0
#define SRV_FUNC_RECV_ID 0x7A1
#define SRV_SEND_ID 0x7A8

#define ISOTP_BUFSIZE 256

static uint8_t isotpPhysRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpPhysSendBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncSendBuf[ISOTP_BUFSIZE];

static IsoTpLink isotpPhysLink;
static IsoTpLink isotpFuncLink;
static Iso14229Server uds;

void hardReset() { printf("server hardReset! %u\n", isotp_user_get_ms()); }

const Iso14229ServerConfig cfg = {
    .phys_recv_id = SRV_PHYS_RECV_ID,
    .func_recv_id = SRV_FUNC_RECV_ID,
    .send_id = SRV_SEND_ID,
    .phys_link = &isotpPhysLink,
    .func_link = &isotpFuncLink,
    .userRDBIHandler = NULL,
    .userWDBIHandler = NULL,
    .userHardReset = hardReset,
    .p2_ms = 50,
    .p2_star_ms = 2000,
    .s3_ms = 5000,
};

Iso14229Server srv;

void simpleServerInit() {
    /* initialize the ISO-TP links */
    isotp_init_link(&isotpPhysLink, SRV_SEND_ID, isotpPhysSendBuf, ISOTP_BUFSIZE, isotpPhysRecvBuf,
                    ISOTP_BUFSIZE);
    isotp_init_link(&isotpFuncLink, SRV_SEND_ID, isotpFuncSendBuf, ISOTP_BUFSIZE, isotpFuncRecvBuf,
                    ISOTP_BUFSIZE);

    Iso14229ServerInit(&srv, &cfg);
    iso14229ServerEnableService(&srv, kSID_ECU_RESET);
}

void simpleServerPeriodicTask() {
    uint32_t arb_id;
    uint8_t data[8];
    uint8_t size;

    Iso14229ServerPoll(&srv);
    if (0 == hostCANRxPoll(&arb_id, data, &size)) {
        iso14229ServerReceiveCAN(&srv, arb_id, data, size);
    }
}
