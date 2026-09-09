#include "tp.h"
#include "uds.h"
#include "util.h"

UDSErr_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, const size_t len, const UDSSDU_t *info) {
    if (NULL == hdl || NULL == hdl->send) {
        return UDS_ERR_INVALID_ARG;
    }
    return hdl->send(hdl, (uint8_t *)buf, len, info);
}

UDSErr_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, const size_t bufsiz, size_t *recvlen,
                   UDSSDU_t *info) {
    if (NULL == hdl || NULL == hdl->recv || NULL == recvlen) {
        return UDS_ERR_INVALID_ARG;
    }
    return hdl->recv(hdl, buf, bufsiz, recvlen, info);
}

UDSErr_t UDSTpPoll(UDSTp_t *hdl) {
    if (NULL == hdl || NULL == hdl->poll) {
        return UDS_ERR_INVALID_ARG;
    }
    return hdl->poll(hdl);
}
