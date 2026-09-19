/** @example isotp_example_streaming.c
 * Chunked reception when a complete message does not fit in RAM.
 */

#include "example_platform.h"
#include "isotp.h"

#define EXAMPLE_TX_ID 0x7E0u
#define EXAMPLE_RX_ID 0x7E8u

static ExampleCan* example_can;
static IsoTpLink   link;
static uint8_t     send_buffer[512];
static uint8_t     receive_chunk[64];

int                isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size) {
    return example_can_send(example_can, arbitration_id, data, size, false, false);
}

uint32_t isotp_user_get_us(void) { return example_get_us(); }

void     isotp_user_debug(const char* message, ...) { (void)message; }

void     example_streaming_init(ExampleCan* can) {
    example_can = can;
    isotp_init_link(&link, EXAMPLE_TX_ID, send_buffer, sizeof(send_buffer), receive_chunk, sizeof(receive_chunk));
}

void example_streaming_process(void) {
    uint32_t arbitration_id;
    uint8_t  frame[ISOTP_CAN_DL_CLASSIC];
    uint8_t  frame_size;
    uint8_t  chunk[sizeof(receive_chunk)];
    uint32_t chunk_size;
    bool     complete;

    if (example_can_receive(example_can, &arbitration_id, frame, &frame_size) && arbitration_id == EXAMPLE_RX_ID) {
        isotp_on_can_message(&link, frame, frame_size);
    }

    isotp_poll(&link);

    if (isotp_receive_streaming(&link, chunk, sizeof(chunk), &chunk_size, &complete) == ISOTP_RET_OK) { example_store_chunk(chunk, chunk_size, complete); }
}
