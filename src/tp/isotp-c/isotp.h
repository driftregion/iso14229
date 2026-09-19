////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#ifndef ISOTPC_H
#define ISOTPC_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
    #include <stdint.h>

extern "C" {
#endif

#include "isotp_config.h"
#include "isotp_defines.h"
#include "isotp_user.h"

/**
 * @file isotp.h
 * @brief Public ISO-TP link and transport API.
 */

/**
 * @defgroup isotp_api Transport API
 * @brief Functions for creating, driving, sending through, and receiving from an ISO-TP link.
 * @{
 */

/**
 * @brief State for one independent, full-duplex ISO-TP conversation.
 *
 * Allocate one link for each conversation that can have independent send or
 * receive state. Initialise it with isotp_init_link() before use and keep the
 * object and its buffers alive for the complete lifetime of the link.
 *
 * Calls that access the same link must be serialised. Applications may observe
 * the documented status/result members and set user_send_can_arg when enabled;
 * all other members are implementation state.
 */
typedef struct IsoTpLink {
    /** @cond ISOTP_INTERNAL */
    /* sender parameters */
    uint32_t            send_arbitration_id;
    uint8_t             tx_dl;

    uint8_t*            send_buffer;
    uint32_t            send_buf_size;
    uint32_t            send_size;
    uint32_t            send_offset;

    uint8_t             send_sn;
    uint32_t            send_bs_remain;
    uint32_t            send_st_min_us;
    uint8_t             send_wtf_count;
    uint32_t            send_timer_st;
    uint32_t            send_timer_bs;
    /** @endcond */
    /** Result of the current or most recent segmented transmission. */
    int32_t             send_protocol_result;
    /** Current IsoTpSendStatusTypes value. */
    uint8_t             send_status;
    /** @cond ISOTP_INTERNAL */

    /* receiver parameters */
    uint32_t            receive_arbitration_id;
    uint8_t             rx_dl;

    uint8_t*            receive_buffer;
    uint32_t            receive_buf_size;
    uint32_t            receive_size;
    uint32_t            receive_offset;

    uint8_t             receive_sn;
    uint8_t             receive_bs_count;
    uint32_t            receive_timer_cr;
    /** @endcond */
    /** Result of the current or most recent receive operation. */
    int                 receive_protocol_result;
    /** Current IsoTpReceiveStatusTypes value. Prefer isotp_receive() to consume completed data. */
    uint8_t             receive_status;
    /** @cond ISOTP_INTERNAL */

#ifdef ISO_TP_ENABLE_STREAMING
    uint32_t            receive_stream_size;
    uint8_t             receive_streaming;
    uint8_t             receive_stream_carry_size;
    uint8_t             receive_stream_carry[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
#endif
    /** @endcond */

#if defined(ISO_TP_USER_SEND_CAN_ARG)
    /**
     * Application value passed to isotp_user_send_can() for this link.
     *
     * Set this after isotp_init_link(), which initially clears it to NULL.
     * A CAN controller or driver context pointer is a typical value.
     */
    void*               user_send_can_arg;
#endif

    /** @cond ISOTP_INTERNAL */
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
    isotp_tx_done_cb    tx_done_cb;
    void*               tx_done_cb_arg;
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    isotp_rx_done_cb    rx_done_cb;
    void*               rx_done_cb_arg;
#endif
    /** @endcond */
} IsoTpLink;

/**
 * @brief Initialise an ISO-TP link and its caller-owned buffers.
 *
 * The complete link object is cleared. Its transmit identifier is set to
 * @p sendid, TX_DL is set to ISO_TP_DEFAULT_TX_DL, and receive and transmit
 * state become idle.
 *
 * @param[out] link Link object to initialise. Must not be NULL.
 * @param[in] sendid CAN arbitration identifier used by isotp_send() and by
 *                   Flow Control responses.
 * @param[in,out] sendbuf Persistent buffer into which outgoing payloads are
 *                        copied. Must not be NULL when @p sendbufsize is nonzero.
 * @param[in] sendbufsize Capacity of @p sendbuf and therefore the maximum
 *                        payload accepted for transmission.
 * @param[in,out] recvbuf Persistent reassembly or streaming buffer. Must not be
 *                        NULL when @p recvbufsize is nonzero.
 * @param[in] recvbufsize Capacity of @p recvbuf. Without streaming, incoming
 *                        messages larger than this are rejected.
 *
 * @pre The link is not being used by another call.
 * @see isotp_set_tx_dl()
 */
void isotp_init_link(IsoTpLink* link, uint32_t sendid, uint8_t* sendbuf, uint32_t sendbufsize, uint8_t* recvbuf, uint32_t recvbufsize);

/**
 * @brief Set the CAN frame data length used to transmit on a link.
 *
 * A Classical CAN link uses 8. CAN FD links may use 12, 16, 20, 24, 32, 48,
 * or 64, subject to the compiled ISO_TP_MAX_CAN_FRAME_SIZE.
 *
 * @param[in,out] link Initialised link, or NULL.
 * @param[in] tx_dl Desired transmit data length.
 * @retval ISOTP_RET_OK The value was applied.
 * @retval ISOTP_RET_ERROR The link is NULL, the value is not a legal CAN data
 *                         length, or it exceeds ISO_TP_MAX_CAN_FRAME_SIZE.
 * @retval ISOTP_RET_INPROGRESS A segmented transmission is active; its TX_DL
 *                              cannot be changed.
 *
 * @note RX_DL is learned independently from each incoming First Frame.
 * @example isotp_example_can_fd.c
 */
int isotp_set_tx_dl(IsoTpLink* link, uint8_t tx_dl);

/**
 * @brief Return the effective transmit data length for a link.
 *
 * @param[in] link Initialised link, or NULL.
 * @return The configured TX_DL, with 8 used as a defensive fallback for a
 *         zero-valued field; returns 0 when @p link is NULL.
 */
uint8_t isotp_get_tx_dl(const IsoTpLink* link);

/**
 * @brief Clear a link's state and callback registrations.
 *
 * No memory is freed because the library owns no allocation. The caller retains
 * ownership of the link and both buffers. Passing NULL has no effect.
 *
 * @param[in,out] link Link to clear, or NULL.
 */
void isotp_destroy_link(IsoTpLink* link);

/**
 * @brief Advance segmented transmission and protocol timeouts.
 *
 * Call this regularly even when completion callbacks are enabled. The required
 * frequency depends on the configured separation time and response timeout.
 * Single-frame transmission and incoming-frame parsing happen synchronously in
 * their respective API calls.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @example isotp_example_polling.c
 */
void isotp_poll(IsoTpLink* link);

/**
 * @brief Process one CAN frame already routed to this link.
 *
 * The application must filter arbitration identifiers before calling this
 * function. Frames shorter than two bytes or longer than
 * ISO_TP_MAX_CAN_FRAME_SIZE are ignored. Valid frames update receive or
 * transmit flow-control state and may synchronously send a Flow Control frame.
 * A registered receive callback may run before this function returns.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @param[in] data Frame payload, valid for at least @p len bytes. Must not be NULL.
 * @param[in] len CAN payload length in bytes.
 */
void isotp_on_can_message(IsoTpLink* link, const uint8_t* data, uint8_t len);

/**
 * @brief Start transmitting a payload with the link's configured CAN identifier.
 *
 * The payload is copied into the link's send buffer. A Single Frame is sent
 * synchronously. For a segmented message, only the First Frame is sent here;
 * isotp_poll() sends the remaining Consecutive Frames after Flow Control.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @param[in] payload Payload to copy. Must be valid for at least @p size bytes.
 * @param[in] size Payload size. It must not exceed the link's send-buffer capacity.
 * @retval ISOTP_RET_OK The Single Frame or First Frame was accepted by the driver.
 * @retval ISOTP_RET_OVERFLOW The payload exceeds the link's send buffer.
 * @retval ISOTP_RET_INPROGRESS Another segmented transmission is active.
 * @return Any other value returned by isotp_user_send_can().
 *
 * @warning ISOTP_RET_OK does not mean a segmented transmission is complete.
 *          Keep polling until the completion callback runs or send_status is
 *          no longer ISOTP_SEND_STATUS_INPROGRESS, then inspect
 *          send_protocol_result.
 */
int isotp_send(IsoTpLink* link, const uint8_t payload[], uint32_t size);

/**
 * @brief Start transmitting with a one-time CAN identifier override.
 *
 * Behaviour and return values are the same as isotp_send(), except @p id is used
 * instead of the identifier stored in the link. This is commonly used for a
 * functional-addressing request. ISO-TP functional requests must fit in a
 * Single Frame; the library does not enforce that addressing rule.
 *
 * @param[in,out] link Initialised link, or NULL.
 * @param[in] id CAN arbitration identifier for this transmission.
 * @param[in] payload Payload to copy. Must be valid for at least @p size bytes.
 * @param[in] size Payload size.
 * @retval ISOTP_RET_ERROR The link is NULL.
 * @retval ISOTP_RET_OK The Single Frame or First Frame was accepted by the driver.
 * @retval ISOTP_RET_OVERFLOW The payload exceeds the link's send buffer.
 * @retval ISOTP_RET_INPROGRESS Another segmented transmission is active.
 * @return Any other value returned by isotp_user_send_can().
 */
int isotp_send_with_id(IsoTpLink* link, uint32_t id, const uint8_t payload[], uint32_t size);

/**
 * @brief Copy and consume one completed, non-streaming message.
 *
 * At most @p payload_size bytes are copied. The completed message is released
 * even when the destination is too small, so any uncopied remainder is lost.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @param[out] payload Destination buffer. Must not be NULL.
 * @param[in] payload_size Capacity of @p payload.
 * @param[out] out_size Number of bytes copied. Must not be NULL.
 * @retval ISOTP_RET_OK A completed message was copied and consumed.
 * @retval ISOTP_RET_NO_DATA No complete message is available.
 * @retval ISOTP_RET_ERROR Streaming reception is active, or a receive callback
 *                         is registered in a build that supports callbacks.
 */
int isotp_receive(IsoTpLink* link, uint8_t* payload, const uint32_t payload_size, uint32_t* out_size);

#ifdef ISO_TP_ENABLE_STREAMING
/**
 * @brief Copy and consume the next available chunk of an incoming message.
 *
 * This function supports both oversized streaming messages and messages that
 * fit in the normal receive buffer. Consuming an intermediate chunk emits a
 * Continue Flow Control frame when more wire data is needed.
 *
 * @param[in,out] link Initialised link, or NULL.
 * @param[out] payload Destination for the complete available chunk, or NULL.
 * @param[in] payload_size Capacity of @p payload.
 * @param[out] out_size Number of bytes copied, or NULL.
 * @param[out] is_complete Set to true when this chunk ends the message, or NULL.
 * @retval ISOTP_RET_OK A chunk was copied.
 * @retval ISOTP_RET_NO_DATA No chunk is currently available.
 * @retval ISOTP_RET_NOSPACE The destination cannot hold the available chunk;
 *                           the chunk remains available.
 * @retval ISOTP_RET_ERROR Any required pointer is NULL.
 * @example isotp_example_streaming.c
 */
int isotp_receive_streaming(IsoTpLink* link, uint8_t* payload, const uint32_t payload_size, uint32_t* out_size, bool* is_complete);
#endif

#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
/**
 * @brief Register or clear the successful-transmission callback.
 *
 * A Single Frame invokes the callback synchronously from isotp_send() or
 * isotp_send_with_id(). A segmented transmission invokes it from isotp_poll()
 * after the final Consecutive Frame is accepted.
 *
 * @param[in,out] link Initialised link. Passing NULL has no effect.
 * @param[in] cb Callback to register, or NULL to disable notification.
 * @param[in] arg Application value passed to @p cb.
 */
void isotp_set_tx_done_cb(IsoTpLink* link, isotp_tx_done_cb cb, void* arg);
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
/**
 * @brief Register or clear the completed-receive callback.
 *
 * The callback runs synchronously from isotp_on_can_message(). Its payload
 * pointer refers to the link's receive buffer and is valid only until the
 * callback returns. Registering it disables delivery through isotp_receive().
 * Oversized streaming messages are still delivered through
 * isotp_receive_streaming().
 *
 * @param[in,out] link Initialised link. Passing NULL has no effect.
 * @param[in] cb Callback to register, or NULL to restore polling delivery.
 * @param[in] arg Application value passed to @p cb.
 * @example isotp_example_callbacks.c
 */
void isotp_set_rx_done_cb(IsoTpLink* link, isotp_rx_done_cb cb, void* arg);
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif // ISOTPC_H
