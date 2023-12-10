#ifndef __ISOTP_H__
#define __ISOTP_H__

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
#include <stdint.h>

extern "C" {
#endif

#include "isotp_defines.h"
#include "isotp_config.h"

/**
 * @brief Struct containing the data for linking an application to a CAN instance.
 * The data stored in this struct is used internally and may be used by software programs
 * using this library.
 */
typedef struct IsoTpLink {
    /* sender paramters */
    uint32_t                    send_arbitration_id; /* used to reply consecutive frame */
    /* message buffer */
    uint8_t*                    send_buffer;
    uint16_t                    send_buf_size;
    uint16_t                    send_size;
    uint16_t                    send_offset;
    /* multi-frame flags */
    uint8_t                     send_sn;
    uint16_t                    send_bs_remain; /* Remaining block size */
    uint8_t                     send_st_min;    /* Separation Time between consecutive frames, unit millis */
    uint8_t                     send_wtf_count; /* Maximum number of FC.Wait frame transmissions  */
    uint32_t                    send_timer_st;  /* Last time send consecutive frame */    
    uint32_t                    send_timer_bs;  /* Time until reception of the next FlowControl N_PDU
                                                   start at sending FF, CF, receive FC
                                                   end at receive FC */
    int                         send_protocol_result;
    uint8_t                     send_status;

    /* receiver paramters */
    uint32_t                    receive_arbitration_id;
    /* message buffer */
    uint8_t*                    receive_buffer;
    uint16_t                    receive_buf_size;
    uint16_t                    receive_size;
    uint16_t                    receive_offset;
    /* multi-frame control */
    uint8_t                     receive_sn;
    uint8_t                     receive_bs_count; /* Maximum number of FC.Wait frame transmissions  */
    uint32_t                    receive_timer_cr; /* Time until transmission of the next ConsecutiveFrame N_PDU
                                                     start at sending FC, receive CF 
                                                     end at receive FC */
    int                         receive_protocol_result;
    uint8_t                     receive_status;                                                     

    /* user implemented callback functions */
    uint32_t                    (*isotp_user_get_ms)(void); /* get millisecond */
    int                         (*isotp_user_send_can)(const uint32_t arbitration_id,
                            const uint8_t* data, const uint8_t size, void *user_data); /* send can message. should return ISOTP_RET_OK when success.  */
    void                        (*isotp_user_debug)(const char* message, ...); /* print debug message */
    void*                       user_data; /* user data */
} IsoTpLink;

/**
 * @brief Initialises the ISO-TP library.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param sendid The ID used to send data to other CAN nodes.
 * @param sendbuf A pointer to an area in memory which can be used as a buffer for data to be sent.
 * @param sendbufsize The size of the buffer area.
 * @param recvbuf A pointer to an area in memory which can be used as a buffer for data to be received.
 * @param recvbufsize The size of the buffer area.
 * @param isotp_user_get_ms A pointer to a function which returns the current time as milliseconds.
 * @param isotp_user_send_can A pointer to a function which sends a can message. should return ISOTP_RET_OK when success.
 * @param isotp_user_debug A pointer to a function which prints a debug message.
 * @param isotp_user_debug A pointer to user data passed to the user implemented callback functions.
 */
void isotp_init_link(
    IsoTpLink *link,
    uint32_t sendid, 
    uint8_t *sendbuf, 
    uint16_t sendbufsize,
    uint8_t *recvbuf,
    uint16_t recvbufsize,
    uint32_t (*isotp_user_get_ms)(void),
    int (*isotp_user_send_can)(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size, void *user_data),
    void (*isotp_user_debug)(const char* message, ...),
    void *user_data
 );

/**
 * @brief Polling function; call this function periodically to handle timeouts, send consecutive frames, etc.
 *
 * @param link The @code IsoTpLink @endcode instance used.
 */
void isotp_poll(IsoTpLink *link);

/**
 * @brief Handles incoming CAN messages.
 * Determines whether an incoming message is a valid ISO-TP frame or not and handles it accordingly.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param data The data received via CAN.
 * @param len The length of the data received.
 */
void isotp_on_can_message(IsoTpLink *link, uint8_t *data, uint8_t len);

/**
 * @brief Sends ISO-TP frames via CAN, using the ID set in the initialising function.
 *
 * Single-frame messages will be sent immediately when calling this function.
 * Multi-frame messages will be sent consecutively when calling isotp_poll.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param payload The payload to be sent. (Up to 4095 bytes).
 * @param size The size of the payload to be sent.
 *
 * @return Possible return values:
 *  - @code ISOTP_RET_OVERFLOW @endcode
 *  - @code ISOTP_RET_INPROGRESS @endcode
 *  - @code ISOTP_RET_OK @endcode
 *  - The return value of the user shim function isotp_user_send_can().
 */
int isotp_send(IsoTpLink *link, const uint8_t payload[], uint16_t size);

/**
 * @brief See @link isotp_send @endlink, with the exception that this function is used only for functional addressing.
 */
int isotp_send_with_id(IsoTpLink *link, uint32_t id, const uint8_t payload[], uint16_t size);

/**
 * @brief Receives and parses the received data and copies the parsed data in to the internal buffer.
 * @param link The @link IsoTpLink @endlink instance used to transceive data.
 * @param payload A pointer to an area in memory where the raw data is copied from.
 * @param payload_size The size of the received (raw) CAN data.
 * @param out_size A reference to a variable which will contain the size of the actual (parsed) data.
 *
 * @return Possible return values:
 *      - @link ISOTP_RET_OK @endlink
 *      - @link ISOTP_RET_NO_DATA @endlink
 */
int isotp_receive(IsoTpLink *link, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size);

#ifdef __cplusplus
}
#endif

#endif // __ISOTP_H__

