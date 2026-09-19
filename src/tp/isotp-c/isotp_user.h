////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#ifndef ISOTPC_USER_H
#define ISOTPC_USER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file isotp_user.h
 * @brief Application-provided platform hooks.
 */

/** @defgroup isotp_platform Platform integration
 * @brief Functions that every application supplies to connect ISO-TP to its driver and clock.
 * @{ */

/**
 * @brief Receive a diagnostic message from the library.
 *
 * The application may implement this as a no-op. Calls can occur from any
 * transport API that detects an error. The library does not require the
 * message to be retained after this function returns.
 *
 * @param[in] message Diagnostic string or printf-style format.
 * @param[in] ... Optional format arguments.
 */
void isotp_user_debug(const char* message, ...);

/**
 * @brief Submit one CAN or CAN FD frame to the application's driver.
 *
 * The implementation must consume or copy @p data before returning. It may be
 * called synchronously from isotp_send(), isotp_on_can_message(),
 * isotp_receive_streaming(), or isotp_poll().
 *
 * @param[in] arbitration_id CAN identifier to transmit.
 * @param[in] data Frame payload, valid only for the duration of this call.
 * @param[in] size Frame payload length. It never exceeds
 *                 ISO_TP_MAX_CAN_FRAME_SIZE. Lengths above 8 require CAN FD.
 * @param[in] flags Present only with ISO_TP_USER_SEND_CAN_FLAGS. A bitwise OR
 *                  of ISOTP_CAN_FRAME_FLAG_FD and ISOTP_CAN_FRAME_FLAG_BRS;
 *                  short frames on a link with TX_DL above 8 still carry the
 *                  FD flag.
 * @param[in] arg Present only with ISO_TP_USER_SEND_CAN_ARG. This is the link's
 *                user_send_can_arg value.
 * @retval ISOTP_RET_OK The driver accepted the frame.
 * @retval ISOTP_RET_NOSPACE The driver is temporarily full and a polled
 *                           Consecutive Frame should be retried.
 * @retval ISOTP_RET_ERROR The frame could not be submitted.
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size
#ifdef ISO_TP_USER_SEND_CAN_FLAGS
                        , const uint8_t flags
#endif
#ifdef ISO_TP_USER_SEND_CAN_ARG
                        , void* arg
#endif
);

/**
 * @brief Return the current 32-bit monotonic time in microseconds.
 *
 * The value must advance independently of call frequency. Natural wraparound
 * at UINT32_MAX is supported.
 *
 * @return Current platform tick in microseconds.
 */
uint32_t isotp_user_get_us(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // ISOTPC_USER_H
