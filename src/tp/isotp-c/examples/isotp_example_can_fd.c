/** @example isotp_example_can_fd.c
 * CAN FD link with frame flags, bit-rate switching and a per-link CAN handle.
 */

#include "example_platform.h"
#include "isotp.h"

#define EXAMPLE_TX_ID 0x7E0u

static IsoTpLink link;
static uint8_t   send_buffer[4095];
static uint8_t   receive_buffer[4095];

int              isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size, const uint8_t flags, void* arg) {
    ExampleCan* can = (ExampleCan*)arg;
    const bool  fd  = (flags & ISOTP_CAN_FRAME_FLAG_FD) != 0u;
    const bool  brs = (flags & ISOTP_CAN_FRAME_FLAG_BRS) != 0u;
    return example_can_send(can, arbitration_id, data, size, fd, brs);
}

uint32_t isotp_user_get_us(void) { return example_get_us(); }

void     isotp_user_debug(const char* message, ...) { (void)message; }

void     example_can_fd_init(ExampleCan* can) {
    isotp_init_link(&link, EXAMPLE_TX_ID, send_buffer, sizeof(send_buffer), receive_buffer, sizeof(receive_buffer));
    link.user_send_can_arg = can;
}

int example_can_fd_use_classical_frames(void) { return isotp_set_tx_dl(&link, ISOTP_CAN_DL_CLASSIC); }
