#include "tp.h"
#include "util.h"

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