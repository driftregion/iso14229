#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/can.h>

#include "iso14229.h"

#define PHYS_RX_ADDR 0x7E0
#define PHYS_TX_ADDR 0x7E8
#define FUNC_RX_ADDR 0x7DF

CAN_MSGQ_DEFINE(can_rxq, 16);

static const struct device *can_dev;

uint32_t isotp_user_get_us(void) { return k_uptime_get_32() * 1000; }

void isotp_user_debug(const char *message, ...) { (void)message; }

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    (void)user_data;
    struct can_frame frame = {.id = arbitration_id, .dlc = size};
    memcpy(frame.data, data, size);
    if (can_send(can_dev, &frame, K_MSEC(100), NULL, NULL)) {
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    (void)srv;
    (void)arg;
    switch (ev) {
    case UDS_EVT_DiagSessCtrl:
    case UDS_EVT_EcuReset:
        return UDS_PositiveResponse;
    case UDS_EVT_DoScheduledReset:
        sys_reboot(SYS_REBOOT_WARM);
        return UDS_OK;
    default:
        return UDS_NRC_ServiceNotSupported;
    }
}

int main(void) {
    static UDSServer_t srv;
    static UDSTpISOTpC_t tp;

    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!device_is_ready(can_dev) || can_start(can_dev)) {
        return -1;
    }

    UDSServerTpISOTpCInit(&tp, PHYS_RX_ADDR, PHYS_TX_ADDR, FUNC_RX_ADDR);

    const struct can_filter phys_filter = {.id = PHYS_RX_ADDR, .mask = CAN_STD_ID_MASK};
    const struct can_filter func_filter = {.id = FUNC_RX_ADDR, .mask = CAN_STD_ID_MASK};
    can_add_rx_filter_msgq(can_dev, &can_rxq, &phys_filter);
    can_add_rx_filter_msgq(can_dev, &can_rxq, &func_filter);

    UDSServerInit(&srv);
    srv.tp = &tp.hdl;
    srv.fn = fn;

    struct can_frame frame;
    while (1) {
        while (k_msgq_get(&can_rxq, &frame, K_NO_WAIT) == 0) {
            if (frame.id == PHYS_RX_ADDR) {
                isotp_on_can_message(&tp.phys_link, frame.data, frame.dlc);
            } else if (frame.id == FUNC_RX_ADDR) {
                isotp_on_can_message(&tp.func_link, frame.data, frame.dlc);
            }
        }
        UDSServerPoll(&srv);
        k_msleep(1);
    }
    return 0;
}
