#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "isotp.h"

#define FUZZ_SEND_BUFFER_SIZE 4096u
#define FUZZ_RECEIVE_BUFFER_SIZE 4096u
#define FUZZ_OUTPUT_BUFFER_SIZE 4096u
#define FUZZ_MAX_OPERATIONS 32u
#define FUZZ_MAX_CAN_SOURCE_SIZE 256u
#define FUZZ_MAX_RECORDED_TX_FRAMES 64u

typedef struct {
        uint32_t arbitration_id;
        uint8_t  data[ISO_TP_MAX_CAN_FRAME_SIZE];
        uint8_t  size;
#ifdef ISO_TP_USER_SEND_CAN_FLAGS
        uint8_t flags;
#endif
} FuzzRecordedCanFrame;

static const uint32_t       fuzz_receive_buffer_sizes[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 12u, 16u, 24u, 32u, 64u, 256u, 4096u};
static const uint32_t       fuzz_output_buffer_sizes[]  = {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 12u, 16u, 32u, 64u, 128u, 512u, 1024u, 4096u};

static uint32_t             fuzz_time_us;
static FuzzRecordedCanFrame fuzz_recorded_tx_frames[FUZZ_MAX_RECORDED_TX_FRAMES];
static uint32_t             fuzz_recorded_tx_frame_count;
static uint32_t             fuzz_dropped_tx_frame_count;
static uint32_t             fuzz_invalid_tx_length_count;
static uint32_t             fuzz_debug_call_count;

static uint32_t             fuzz_read_u32_le(const uint8_t* data) {
    return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8u) | ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static size_t fuzz_min_size(size_t left, size_t right) { return left < right ? left : right; }

static void   fuzz_reset_platform_state(void) {
    fuzz_time_us                 = 0u;
    fuzz_recorded_tx_frame_count = 0u;
    fuzz_dropped_tx_frame_count  = 0u;
    fuzz_invalid_tx_length_count = 0u;
    fuzz_debug_call_count        = 0u;
    (void)memset(fuzz_recorded_tx_frames, 0, sizeof(fuzz_recorded_tx_frames));
}

uint32_t isotp_user_get_us(void) {
    /* Fuzz iterations use deterministic time so timeout paths remain reproducible. */
    return fuzz_time_us;
}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size
#ifdef ISO_TP_USER_SEND_CAN_FLAGS
                        ,
                        const uint8_t flags
#endif
#ifdef ISO_TP_USER_SEND_CAN_ARG
                        ,
                        void* arg
#endif
) {
#ifdef ISO_TP_USER_SEND_CAN_ARG
    (void)arg;
#endif

    if (size > ISO_TP_MAX_CAN_FRAME_SIZE) {
        ++fuzz_invalid_tx_length_count;
        return ISOTP_RET_ERROR;
    }

    if (fuzz_recorded_tx_frame_count >= FUZZ_MAX_RECORDED_TX_FRAMES) {
        ++fuzz_dropped_tx_frame_count;
        return ISOTP_RET_OK;
    }

    fuzz_recorded_tx_frames[fuzz_recorded_tx_frame_count].arbitration_id = arbitration_id;
    fuzz_recorded_tx_frames[fuzz_recorded_tx_frame_count].size           = size;
#ifdef ISO_TP_USER_SEND_CAN_FLAGS
    fuzz_recorded_tx_frames[fuzz_recorded_tx_frame_count].flags = flags;
#endif
    if (data != NULL && size > 0u) { (void)memcpy(fuzz_recorded_tx_frames[fuzz_recorded_tx_frame_count].data, data, size); }
    ++fuzz_recorded_tx_frame_count;

    return ISOTP_RET_OK;
}

void isotp_user_debug(const char* message, ...) {
    (void)message;
    /* Debug messages can contain bytes derived from fuzz input, so the harness only counts them. */
    ++fuzz_debug_call_count;
}

static void fuzz_deliver_can_frame(IsoTpLink* link, const uint8_t** cursor, const uint8_t* end) {
    uint8_t declared_len;
    uint8_t data_len;
    size_t  available;
    size_t  copy_len;
    uint8_t frame[FUZZ_MAX_CAN_SOURCE_SIZE];

    if ((size_t)(end - *cursor) < 2u) {
        *cursor = end;
        return;
    }

    declared_len = *(*cursor)++;
    data_len     = *(*cursor)++;

    /*
     * The frame source is 256 bytes because the public API takes an 8-bit
     * length. This keeps every declared length backed by accessible storage,
     * while zero-filled bytes let truncated fuzzer inputs remain protocol input
     * instead of becoming harness out-of-bounds reads.
     */
    (void)memset(frame, 0, sizeof(frame));
    available = (size_t)(end - *cursor);
    copy_len  = fuzz_min_size((size_t)data_len, available);
    copy_len  = fuzz_min_size(copy_len, sizeof(frame));
    if (copy_len > 0u) { (void)memcpy(frame, *cursor, copy_len); }

    *cursor += copy_len;
    isotp_on_can_message(link, frame, declared_len);

    if (copy_len < (size_t)data_len) { *cursor = end; }
}

static void fuzz_advance_time(const uint8_t** cursor, const uint8_t* end) {
    if ((size_t)(end - *cursor) < 4u) {
        *cursor = end;
        return;
    }

    /* Natural uint32_t wraparound is part of the time model used by isotp-c. */
    fuzz_time_us += fuzz_read_u32_le(*cursor);
    *cursor += 4u;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    IsoTpLink      link;
    uint8_t        send_buffer[FUZZ_SEND_BUFFER_SIZE];
    uint8_t        receive_buffer[FUZZ_RECEIVE_BUFFER_SIZE];
    uint8_t        output_buffer[FUZZ_OUTPUT_BUFFER_SIZE];
    uint32_t       receive_buffer_size;
    uint32_t       operation_count;
    const uint8_t* cursor;
    const uint8_t* end;

    if (data == NULL || size == 0u) { return 0; }

    fuzz_reset_platform_state();
    (void)memset(send_buffer, 0, sizeof(send_buffer));
    (void)memset(receive_buffer, 0, sizeof(receive_buffer));
    (void)memset(output_buffer, 0, sizeof(output_buffer));

    receive_buffer_size = fuzz_receive_buffer_sizes[data[0] & 0x0fu];
    isotp_init_link(&link, 0x731u, send_buffer, sizeof(send_buffer), receive_buffer, receive_buffer_size);

    cursor = data + 1u;
    end    = data + size;

    /*
     * Malformed ISO-TP frames are expected fuzz inputs. The harness limits only
     * parser work and memory access; protocol rejection is left to the library.
     */
    for (operation_count = 0u; operation_count < FUZZ_MAX_OPERATIONS && cursor < end; ++operation_count) {
        uint8_t operation = *cursor++;

        switch (operation) {
            case 0x00u: fuzz_deliver_can_frame(&link, &cursor, end); break;

            case 0x01u: fuzz_advance_time(&cursor, end); break;

            case 0x02u: isotp_poll(&link); break;

            case 0x03u:
                if (cursor < end) {
                    uint32_t output_size;
                    uint32_t received_size = 0u;

                    output_size            = fuzz_output_buffer_sizes[*cursor++ & 0x0fu];
                    (void)isotp_receive(&link, output_buffer, output_size, &received_size);
                } else {
                    cursor = end;
                }
                break;

            default: break;
        }
    }

    return 0;
}
