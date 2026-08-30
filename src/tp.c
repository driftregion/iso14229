#include "tp.h"
#include "util.h"

UDSTpSize_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, UDSTpSize_t len, const UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->send);
    return hdl->send(hdl, (uint8_t *)buf, len, info);
}

UDSTpSize_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->recv);
    return hdl->recv(hdl, buf, bufsize, info);
}

UDSTpStatus_t UDSTpPoll(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->poll);
    return hdl->poll(hdl);
}