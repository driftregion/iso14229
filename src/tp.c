#include "tp.h"
#include "uds.h"
#include "util.h"

UDSErr_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, const size_t len, const UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->send);
    UDS_ASSERT(buf);
    return hdl->send(hdl, buf, len, info);
}

UDSErr_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, const size_t bufsiz, size_t *recvlen,
                   UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->recv);
    UDS_ASSERT(recvlen);
    return hdl->recv(hdl, buf, bufsiz, recvlen, info);
}

UDSErr_t UDSTpPoll(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(hdl->poll);
    return hdl->poll(hdl);
}
