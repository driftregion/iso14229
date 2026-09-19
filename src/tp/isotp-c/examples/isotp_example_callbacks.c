/** @example isotp_example_callbacks.c
 * Receive and transmit completion callbacks.
 */

#include "example_platform.h"
#include "isotp.h"

#define EXAMPLE_TX_ID 0x7E0u
#define EXAMPLE_RX_ID 0x7E8u

static ExampleCan* example_can;
static IsoTpLink   link;
static uint8_t     send_buffer[4095];
static uint8_t     receive_buffer[4095];

int                isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size) {
    return example_can_send(example_can, arbitration_id, data, size, false, false);
}

uint32_t    isotp_user_get_us(void) { return example_get_us(); }

void        isotp_user_debug(const char* message, ...) { (void)message; }

static void transmit_complete(void* completed_link, uint32_t size, void* user_arg) {
    (void)completed_link;
    (void)size;
    (void)user_arg;
}

static void receive_complete(void* completed_link, const uint8_t* data, uint32_t size, void* user_arg) {
    (void)completed_link;
    (void)user_arg;
    example_process_message(data, size);
}

void example_callbacks_init(ExampleCan* can) {
    example_can = can;
    isotp_init_link(&link, EXAMPLE_TX_ID, send_buffer, sizeof(send_buffer), receive_buffer, sizeof(receive_buffer));
    isotp_set_tx_done_cb(&link, transmit_complete, NULL);
    isotp_set_rx_done_cb(&link, receive_complete, NULL);
}

void example_callbacks_process(void) {
    uint32_t arbitration_id;
    uint8_t  frame[ISOTP_CAN_DL_CLASSIC];
    uint8_t  frame_size;

    if (example_can_receive(example_can, &arbitration_id, frame, &frame_size) && arbitration_id == EXAMPLE_RX_ID) {
        isotp_on_can_message(&link, frame, frame_size);
    }

    isotp_poll(&link);
}
