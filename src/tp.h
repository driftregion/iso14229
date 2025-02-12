#pragma once

#include "sys.h"

#if defined UDS_TP_ISOTP_C_SOCKETCAN
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif
#endif

enum UDSTpStatusFlags {
    UDS_TP_IDLE = 0x0000,
    UDS_TP_SEND_IN_PROGRESS = 0x0001,
    UDS_TP_RECV_COMPLETE = 0x0002,
    UDS_TP_ERR = 0x0004,
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
    uint32_t A_SA;         // application source address
    uint32_t A_TA;         // application target address
    UDS_A_TA_Type_t A_TA_Type; // application target address type (physical or functional)
    uint32_t A_AE;             // application layer remote address
} UDSSDU_t;

#define UDS_TP_NOOP_ADDR (0xFFFFFFFF)

/**
 * @brief UDS Transport layer
 * @note implementers should embed this struct at offset zero in their own transport layer handle
 */
typedef struct UDSTp {
    /**
     * @brief Send data to the transport
     * @param hdl: pointer to transport handle
     * @param buf: a pointer to the data to send (this may be the buffer returned by @ref
     * get_send_buf)
     * @param info: pointer to SDU info (may be NULL). If NULL, implementation should send with
     * physical addressing
     */
    ssize_t (*send)(struct UDSTp *hdl, uint8_t *buf, size_t len, UDSSDU_t *info);

    /**
     * @brief Receive data from the transport
     * @param hdl: transport handle
     * @param buf: receive buffer
     * @param bufsize: size of the receive buffer
     * @param info: pointer to SDU info to be updated by transport implementation. May be NULL. If non-NULL, the transport implementation must populate it with valid values.
     */
    ssize_t (*recv)(struct UDSTp *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info);

    /**
     * @brief Poll the transport layer.
     * @param hdl: pointer to transport handle
     * @note the transport layer user is responsible for calling this function periodically
     * @note threaded implementations like linux isotp sockets don't need to do anything here.
     * @return UDS_TP_IDLE if idle, otherwise UDS_TP_SEND_IN_PROGRESS or UDS_TP_RECV_COMPLETE
     */
    UDSTpStatus_t (*poll)(struct UDSTp *hdl);

    /**
     * @brief Peek at the received data
     * @param hdl: pointer to transport handle
     * @param buf: set to the received data
     * @param info: filled with SDU info by the callee if not NULL
     * @return size of received data on success, -1 on error
     * @note The transport will be unable to receive further data until @ref ack_recv is called
     * @note The **buf returned by peek is valid until @ref ack_recv is called
     */
    ssize_t (*peek)(struct UDSTp *hdl, uint8_t **buf, UDSSDU_t *info);

    /**
     * @brief Acknowledge that the received data has been processed and may be discarded
     * @param hdl: pointer to transport handle
     * @note: after ack_recv() is called and before new messages are received, peek must return 0.
     */
    void (*ack_recv)(struct UDSTp *hdl);

    /**
     * @brief Get the transport layer's send buffer
     * @param hdl: pointer to transport handle
     * @param buf: double pointer which will be pointed to the send buffer
     * @return size of transport layer's send buffer on success, -1 on error
     */
    ssize_t (*get_send_buf)(struct UDSTp *hdl, uint8_t **p_buf);
} UDSTp_t;

ssize_t UDSTpGetSendBuf(UDSTp_t *hdl, uint8_t **buf);
ssize_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, ssize_t len, UDSSDU_t *info);
ssize_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info);
UDSTpStatus_t UDSTpPoll(UDSTp_t *hdl);
ssize_t UDSTpPeek(struct UDSTp *hdl, uint8_t **buf, UDSSDU_t *info);
const uint8_t *UDSTpGetRecvBuf(UDSTp_t *hdl, size_t *len);
size_t UDSTpGetRecvLen(UDSTp_t *hdl);
