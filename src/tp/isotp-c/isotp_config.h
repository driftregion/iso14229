////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
//                      ___ ___  _  _ ___ ___ ___                     //
//                     / __/ _ \| \| | __|_ _/ __|                    //
//                    | (_| (_) | .` | _| | | (_ |                    //
//                     \___\___/|_|\_|_| |___\___|                    //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#ifndef ISOTPC_CONFIG_H
#define ISOTPC_CONFIG_H

/**
 * @file isotp_config.h
 * @brief Compile-time transport configuration and defaults.
 *
 * Prefer the corresponding CMake or Make settings when using a supplied build
 * system. Direct builds must define ABI-affecting options identically while
 * compiling the library and every consumer.
 */

/** @defgroup isotp_config Compile-time configuration
 * @brief Macros controlling frame sizes, timing, optional APIs, and platform integration.
 * @{ */

/** The maximum amount of data bytes a single CAN frame may carry (CAN_DL).
 * Classical CAN is limited to 8 bytes; CAN FD additionally allows frames of
 * 12, 16, 20, 24, 32, 48 and 64 bytes.
 *
 * Set this to one of the CAN FD lengths to enable CAN FD support. This
 * increases the size of the internal frame buffers accordingly, so leave it at
 * 8 on platforms without CAN FD.
 */
#ifndef ISO_TP_MAX_CAN_FRAME_SIZE
    #define ISO_TP_MAX_CAN_FRAME_SIZE 8
#endif

/** The CAN_DL (TX_DL) used by a freshly initialised link.
 * This may be reduced per link at runtime using isotp_set_tx_dl(), e.g. when a
 * peer only supports Classical CAN frame lengths.
 */
#ifndef ISO_TP_DEFAULT_TX_DL
    #define ISO_TP_DEFAULT_TX_DL ISO_TP_MAX_CAN_FRAME_SIZE
#endif

/** Flow Control block size advertised by the receiver.
 * A value of 0 asks the sender not to wait for further block acknowledgements.
 */
#ifndef ISO_TP_DEFAULT_BLOCK_SIZE
    #define ISO_TP_DEFAULT_BLOCK_SIZE 8
#endif

/** Minimum Consecutive Frame separation requested by the receiver, in microseconds.
 */
#ifndef ISO_TP_DEFAULT_ST_MIN_US
    #define ISO_TP_DEFAULT_ST_MIN_US 0
#endif

/** Maximum number of Flow Control Wait frames accepted during transmission.
 */
#ifndef ISO_TP_MAX_WFT_NUMBER
    #define ISO_TP_MAX_WFT_NUMBER 1
#endif

/** Timeout for required Flow Control or Consecutive Frames, in microseconds.
 * Keep this below half the range of the 32-bit microsecond clock so wrapping
 * deadline comparisons remain unambiguous.
 */
#ifndef ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US
    #define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US 100000
#endif

/** @def ISO_TP_FRAME_PADDING
 * Pad transmitted Classical CAN frames to TX_DL. CAN FD frames larger than
 * eight bytes are always padded to a legal CAN FD data length.
 */
#ifdef DOXYGEN
    #define ISO_TP_FRAME_PADDING
#endif

/** @def ISO_TP_NO_FORMATTED_ERRORS
 * Omit the two formatted error messages, which are the library's only
 * use of snprintf(). Define this on a target whose libc has no snprintf, or
 * where the 128-byte ISOTP_MAX_ERROR_MSG_SIZE stack buffer is unwelcome. The
 * errors are still reported through isotp_user_debug(), without the values.
 *
 * Measured on Cortex-M4, -Os -DNDEBUG: .text 2235 -> 2091 bytes, and the
 * largest stack frame 160 -> 32 bytes. With this and NDEBUG the object needs
 * nothing from libc but memcpy and memset.
 */
#ifdef DOXYGEN
    #define ISO_TP_NO_FORMATTED_ERRORS
#endif


/** Byte written into unused padded frame positions. */
#ifndef ISO_TP_FRAME_PADDING_VALUE
    #define ISO_TP_FRAME_PADDING_VALUE 0xAA
#endif

/** @def ISO_TP_USER_SEND_CAN_ARG
 * Append the link's user_send_can_arg value to isotp_user_send_can(). This
 * changes the shim signature and the public IsoTpLink layout.
 */
#ifdef DOXYGEN
    #define ISO_TP_USER_SEND_CAN_ARG
#endif

/** @def ISO_TP_USER_SEND_CAN_FLAGS
 * Add a frame-flags argument to isotp_user_send_can(), telling the driver whether a frame has to be
 * transmitted as a CAN FD frame. Enable this if the driver cannot derive the
 * frame format from the frame length. When combined with
 * ISO_TP_USER_SEND_CAN_ARG, the flags argument comes first.
 */
#ifdef DOXYGEN
    #define ISO_TP_USER_SEND_CAN_FLAGS
#endif

/** @def ISO_TP_CAN_FD_USE_BRS
 * Add ISOTP_CAN_FRAME_FLAG_BRS to CAN FD transmissions. This has an effect only
 * with ISO_TP_USER_SEND_CAN_FLAGS.
 */
#ifdef DOXYGEN
    #define ISO_TP_CAN_FD_USE_BRS
#endif

/** @def ISO_TP_TRANSMIT_COMPLETE_CALLBACK
 * Add the transmit callback type, registration API, and link state.
 */
#ifdef DOXYGEN
    #define ISO_TP_TRANSMIT_COMPLETE_CALLBACK
#endif

/** @def ISO_TP_RECEIVE_COMPLETE_CALLBACK
 * Add the receive callback type, registration API, and link state.
 */
#ifdef DOXYGEN
    #define ISO_TP_RECEIVE_COMPLETE_CALLBACK
#endif

/** @def ISO_TP_ENABLE_STREAMING
 * Add isotp_receive_streaming() and link state for receiving messages larger
 * than the receive buffer in application-consumable chunks.
 */
#ifdef DOXYGEN
    #define ISO_TP_ENABLE_STREAMING
#endif

/** @} */

#endif // ISOTPC_CONFIG_H
