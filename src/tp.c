#include "tp.h"
#include "util.h"

/**
 * @brief
 *
 * @param hdl
 * @param info, if NULL, the default values are used:
 *   A_Mtype: message type (diagnostic (DEFAULT), remote diagnostic, secure diagnostic, secure
 * remote diagnostic)
 * A_TA_Type: application target address type (physical (DEFAULT) or functional)
 * A_SA: unused
 * A_TA: unused
 * A_AE: unused
 * @return ssize_t
 */
ssize_t UDSTpGetSendBuf(struct UDSTp *hdl, uint8_t **buf) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->get_send_buf);
    return hdl->get_send_buf(hdl, buf);
}


ssize_t UDSTpPeek(struct UDSTp *hdl, uint8_t **buf, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->peek);
    return hdl->peek(hdl, buf, info);
}

const uint8_t *UDSTpGetRecvBuf(struct UDSTp *hdl, size_t *p_len) {
    UDS_ASSERT(hdl);
    ssize_t len = 0;
    uint8_t *buf = NULL;
    len = UDSTpPeek(hdl, &buf, NULL);
    if (len > 0) {
        if (p_len) {
            *p_len = len;
        }
        return buf;
    } else {
        return NULL;
    }
}

size_t UDSTpGetRecvLen(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    size_t len = 0;
    UDSTpGetRecvBuf(hdl, &len);
    return len;
}

ssize_t UDSTpSend(struct UDSTp *hdl, const uint8_t *buf, ssize_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->send);
    return hdl->send(hdl, (uint8_t *)buf, len, info);
}

ssize_t UDSTpRecv(struct UDSTp *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->recv);
    return hdl->recv(hdl, buf, bufsize, info);
}

UDSTpStatus_t UDSTpPoll(struct UDSTp *hdl) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->poll);
    return hdl->poll(hdl);
}