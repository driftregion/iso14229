#if defined(UDS_TP_ISOTP_C)

#include "util.h"
#include "tp/isotp_c.h"
#include "tp/isotp-c/isotp.h"

static UDSTpStatus_t tp_poll(UDSTpHandle_t *hdl) {
    UDS_ASSERT(hdl);
    UDSTpStatus_t status = 0;
    UDSISOTpC_t *impl = (UDSISOTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

static int peek_link(IsoTpLink *link, uint8_t *buf, size_t bufsize, bool functional) {
    UDS_ASSERT(link);
    UDS_ASSERT(buf);
    int ret = -1;
    switch (link->receive_status) {
    case ISOTP_RECEIVE_STATUS_IDLE:
        ret = 0;
        goto done;
    case ISOTP_RECEIVE_STATUS_INPROGRESS:
        ret = 0;
        goto done;
    case ISOTP_RECEIVE_STATUS_FULL:
        ret = link->receive_size;
        UDS_LOGI(__FILE__, "The link is full. Copying %d bytes\n", ret);
        memmove(buf, link->receive_buffer, link->receive_size);
        break;
    default:
        UDS_LOGI(__FILE__, "receive_status %d not implemented\n", link->receive_status);
        ret = -1;
        goto done;
    }
done:
    return ret;
}

static ssize_t tp_peek(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(p_buf);
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    if (ISOTP_RECEIVE_STATUS_FULL == tp->phys_link.receive_status) { // recv not yet acked
        *p_buf = tp->recv_buf;
        return tp->phys_link.receive_size;
    }
    int ret = -1;
    ret = peek_link(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), false);
    UDS_A_TA_Type_t ta_type = UDS_A_TA_TYPE_PHYSICAL;
    uint32_t ta = tp->phys_ta;
    uint32_t sa = tp->phys_sa;

    if (ret > 0) {
        UDS_LOGI(__FILE__, "just got %d bytes\n", ret);
        ta = tp->phys_sa;
        sa = tp->phys_ta;
        ta_type = UDS_A_TA_TYPE_PHYSICAL;
        *p_buf = tp->recv_buf;
        goto done;
    } else if (ret < 0) {
        goto done;
    } else {
        ret = peek_link(&tp->func_link, tp->recv_buf, sizeof(tp->recv_buf), true);
        if (ret > 0) {
            UDS_LOGI(__FILE__, "just got %d bytes on func link \n", ret);
            ta = tp->func_sa;
            sa = tp->func_ta;
            ta_type = UDS_A_TA_TYPE_FUNCTIONAL;
            *p_buf = tp->recv_buf;
            goto done;
        } else if (ret < 0) {
            goto done;
        }
    }
done:
    if (ret > 0) {
        if (info) {
            info->A_TA = ta;
            info->A_SA = sa;
            info->A_TA_Type = ta_type;
        }
    }
    return ret;
}

static ssize_t tp_send(UDSTpHandle_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ssize_t ret = -1;
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;
    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &tp->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &tp->func_link;
        if (len > 7) {
            UDS_LOGI(__FILE__, "Cannot send more than 7 bytes via functional addressing\n");
            ret = -3;
            goto done;
        }
        break;
    default:
        ret = -4;
        goto done;
    }

    int send_status = isotp_send(link, buf, len);
    switch (send_status) {
    case ISOTP_RET_OK:
        ret = len;
        goto done;
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        ret = send_status;
        goto done;
    }
done:
    return ret;
}

static void tp_ack_recv(UDSTpHandle_t *hdl) {
    UDS_LOGI(__FILE__, "ack recv\n");
    UDS_ASSERT(hdl);
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    uint16_t out_size = 0;
    isotp_receive(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), &out_size);
}

static ssize_t tp_get_send_buf(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    UDS_ASSERT(hdl);
    UDSISOTpC_t *tp = (UDSISOTpC_t *)hdl;
    *p_buf = tp->send_buf;
    return sizeof(tp->send_buf);
}

UDSErr_t UDSISOTpCInit(UDSISOTpC_t *tp, const UDSISOTpCConfig_t *cfg) {
    if (cfg == NULL || tp == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    tp->hdl.poll = tp_poll;
    tp->hdl.send = tp_send;
    tp->hdl.peek = tp_peek;
    tp->hdl.ack_recv = tp_ack_recv;
    tp->hdl.get_send_buf = tp_get_send_buf;
    tp->phys_sa = cfg->source_addr;
    tp->phys_ta = cfg->target_addr;
    tp->func_sa = cfg->source_addr_func;
    tp->func_ta = cfg->target_addr_func;

    isotp_init_link(&tp->phys_link, tp->phys_ta, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
    isotp_init_link(&tp->func_link, tp->func_ta, tp->recv_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
    return UDS_OK;
}

#endif
