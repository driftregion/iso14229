#pragma once

#include "sys.h"

#if defined UDS_TP_ISOTP_C_SOCKETCAN
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif
#endif

enum UDSTpStatusFlags {
    UDS_TP_IDLE = 0x00000000,
    UDS_TP_SEND_IN_PROGRESS = 0x00000001,
    UDS_TP_RECV_COMPLETE = 0x00000002,
};

typedef uint32_t UDSTpStatus_t;

typedef enum {
    UDS_A_MTYPE_DIAG = 0,
    UDS_A_MTYPE_REMOTE_DIAG,
    UDS_A_MTYPE_SECURE_DIAG,
    UDS_A_MTYPE_SECURE_REMOTE_DIAG,
} UDS_A_Mtype_t;

typedef enum {
    UDS_A_TA_TYPE_PHYSICAL = 0, // unicast (1:1)
    UDS_A_TA_TYPE_FUNCTIONAL,   // multicast
} UDS_A_TA_Type_t;

typedef uint8_t UDSTpAddr_t;

/**
 * @brief Service data unit (SDU)
 * @details data interface between the application layer and the transport layer
 */
typedef struct {
    UDS_A_Mtype_t A_Mtype; // message type (diagnostic, remote diagnostic, secure diagnostic, secure
                           // remote diagnostic)
    uint16_t A_SA;         // application source address
    uint16_t A_TA;         // application target address
    UDS_A_TA_Type_t A_TA_Type; // application target address type (physical or functional)
    uint16_t A_AE;             // application layer remote address
} UDSSDU_t;

#define UDS_TP_NOOP_ADDR (0xFFFFFFFF)

/**
 * @brief Interface to OSI layer 4 (transport layer)
 * @note implementers should embed this struct at offset zero in their own transport layer handle
 */
typedef struct UDSTpHandle {
    /**
     * @brief Get the transport layer's send buffer
     * @param hdl: pointer to transport handle
     * @param buf: double pointer which will be pointed to the send buffer
     * @return size of transport layer's send buffer on success, -1 on error
     */
    ssize_t (*get_send_buf)(struct UDSTpHandle *hdl, uint8_t **p_buf);

    /**
     * @brief Send the data in the buffer buf
     * @param hdl: pointer to transport handle
     * @param buf: a pointer to the data to send (this may be the buffer returned by @ref
     * get_send_buf)
     * @param info: pointer to SDU info (may be NULL). If NULL, implementation should send with
     * physical addressing
     */
    ssize_t (*send)(struct UDSTpHandle *hdl, uint8_t *buf, size_t len, UDSSDU_t *info);

    /**
     * @brief Poll the transport layer.
     * @param hdl: pointer to transport handle
     * @note the transport layer user is responsible for calling this function periodically
     * @note threaded implementations like linux isotp sockets don't need to do anything here.
     * @return UDS_TP_IDLE if idle, otherwise UDS_TP_SEND_IN_PROGRESS or UDS_TP_RECV_COMPLETE
     */
    UDSTpStatus_t (*poll)(struct UDSTpHandle *hdl);

    /**
     * @brief Peek at the received data
     * @param hdl: pointer to transport handle
     * @param buf: set to the received data
     * @param info: filled with SDU info by the callee if not NULL
     * @return size of received data on success, -1 on error
     * @note The transport will be unable to receive further data until @ref ack_recv is called
     * @note The information returned by peek will not change until @ref ack_recv is called
     */
    ssize_t (*peek)(struct UDSTpHandle *hdl, uint8_t **buf, UDSSDU_t *info);

    /**
     * @brief Acknowledge that the received data has been processed and may be discarded
     * @param hdl: pointer to transport handle
     * @note: after ack_recv() is called and before new messages are received, peek must return 0.
     */
    void (*ack_recv)(struct UDSTpHandle *hdl);
} UDSTpHandle_t;

ssize_t UDSTpGetSendBuf(UDSTpHandle_t *hdl, uint8_t **buf);
ssize_t UDSTpSend(UDSTpHandle_t *hdl, const uint8_t *buf, ssize_t len, UDSSDU_t *info);
UDSTpStatus_t UDSTpPoll(UDSTpHandle_t *hdl);
ssize_t UDSTpPeek(struct UDSTpHandle *hdl, uint8_t **buf, UDSSDU_t *info);
const uint8_t *UDSTpGetRecvBuf(UDSTpHandle_t *hdl, size_t *len);
size_t UDSTpGetRecvLen(UDSTpHandle_t *hdl);
void UDSTpAckRecv(UDSTpHandle_t *hdl);
