#include <stdint.h>
#include "bsp.h"
#include "iso14229.h"
#include "tp/isotp_c.h"

UDSServer_t server;
UDSTpISOTpC_t tp;

static int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data,
                               const uint8_t size, void *user_data) {
    if (0 != BSPSendCAN(arbitration_id, data, size)) {
        return ISOTP_RET_ERROR;
    } else {
        return ISOTP_RET_OK;
    }
}

static void isotp_debug(const char *msg, ...) {
    // BSPLog("%s", msg);
}

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    return kPositiveResponse;
}

int main() {
    BSPInit();
    UDSServerInit(&server);
    server.fn = fn;
    server.tp = &tp.hdl;

    UDSTpISOTpCInit(&tp, &(UDSTpISOTpCConfig_t){
                             .source_addr = 0x7E8,
                             .target_addr = 0x7E0,
                             .source_addr_func = 0x7DF,
                             .target_addr_func = 0,
                             .user_data = NULL,
                             .isotp_user_send_can = isotp_user_send_can,
                             .isotp_user_debug = isotp_debug,
                         });

    uint8_t data[] = {0xf0, 0xf0};
    while (1) {
        UDSServerPoll(&server);
        BSPSetLED(0, true);
        for (volatile int i = 0; i < 1000000; i++) {
            __asm__("nop");
        }
        BSPSetLED(0, false);
        for (volatile int i = 0; i < 1000000; i++) {
            __asm__("nop");
        }
        BSPSendCAN(0x123, data, sizeof(data));
    }
}