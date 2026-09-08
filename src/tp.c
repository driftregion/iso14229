#include "tp.h"
#include "uds.h"
#include "util.h"

UDSTpSsize_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, size_t len, const UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->send);
    if (NULL==hdl || NULL == hdl->send) {
        return UDS_ERR_INVALID_ARG;
    }
    return hdl->send(hdl, (uint8_t *)buf, len, info);
}

UDSTpSsize_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->recv);
    if (NULL==hdl || NULL == hdl->recv) {
        return UDS_ERR_INVALID_ARG;
    }
    return hdl->recv(hdl, buf, len, info);
}

UDSTpStatus_t UDSTpPoll(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->poll);
    return hdl->poll(hdl);
}