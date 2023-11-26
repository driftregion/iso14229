#include "tp/isotp_c.h"
#include "tp/isotp-c/isotp.h"
#include <assert.h>
#include <stdint.h>

static UDSTpStatus_t tp_poll(UDSTpHandle_t *hdl) {
    assert(hdl);
    UDSTpStatus_t status = 0;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

int peek_link(IsoTpLink *link, uint8_t *buf, size_t bufsize, bool functional) {
    assert(link);
    assert(buf);
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
        printf("The link is full. Copying %d bytes\n", ret);
        memmove(buf, link->receive_buffer, link->receive_size);
        break;
    default:
        UDS_DBG_PRINT("receive_status %d not implemented\n", link->receive_status);
        ret = -1;
        goto done;
    }
done:
    return ret;
}

static ssize_t tp_peek(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    assert(hdl);
    assert(p_buf);
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
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
        printf("just got %d bytes\n", ret);
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
            printf("just got %d bytes on func link \n", ret);
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
    assert(hdl);
    ssize_t ret = -1;
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;
    const uint32_t ta = ta_type == UDS_A_TA_TYPE_PHYSICAL ? tp->phys_ta : tp->func_ta;
    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &tp->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &tp->func_link;
        if (len > 7) {
            UDS_DBG_PRINT("Cannot send more than 7 bytes via functional addressing\n");
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
    assert(hdl);
    printf("ack recv\n");
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
}

static ssize_t tp_get_send_buf(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    assert(hdl);
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    *p_buf = tp->send_buf;
    return sizeof(tp->send_buf);
}

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, UDSTpISOTpCConfig_t *cfg) {
    if (cfg == NULL || tp == NULL) {
        return UDS_ERR;
    }
    tp->hdl.poll = tp_poll;
    tp->hdl.send = tp_send;
    tp->hdl.peek = tp_peek;
    tp->hdl.ack_recv = tp_ack_recv;
    tp->hdl.get_send_buf = tp_get_send_buf;
    tp->phys_sa = cfg->source_addr;
    tp->phys_ta = cfg->target_addr;
    tp->func_sa = cfg->source_addr_func;
    tp->func_ta = cfg->target_addr;

    isotp_init_link(&tp->phys_link, tp->phys_ta, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf), UDSMillis, cfg->isotp_user_send_can,
                    cfg->isotp_user_debug, cfg->user_data);
    isotp_init_link(&tp->func_link, tp->func_ta, tp->recv_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf), UDSMillis, cfg->isotp_user_send_can,
                    cfg->isotp_user_debug, cfg->user_data);
    return UDS_OK;
}