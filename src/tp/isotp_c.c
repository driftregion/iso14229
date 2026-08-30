#if defined(UDS_TP_ISOTP_C)

#include "util.h"
#include "log.h"
#include "tp/isotp_c.h"
#include "tp/isotp-c/isotp.h"

static UDSTpStatus_t tp_poll(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    UDSTpStatus_t status = 0;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    isotp_poll(&impl->func_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

static UDSTpSize_t tp_send(UDSTp_t *hdl, const uint8_t *buf, size_t len, const UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDSTpSize_t ret = -1;
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
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

static UDSTpSize_t tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(buf);
    uint16_t out_size = 0;
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;

    int ret = isotp_receive(&tp->phys_link, buf, bufsize, &out_size);
    if (ret == ISOTP_RET_OK) {
        UDS_LOGI(__FILE__, "phys link received %d bytes", out_size);
        if (NULL != info) {
            info->A_TA = tp->phys_sa;
            info->A_SA = tp->phys_ta;
            info->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
        }
    } else if (ret == ISOTP_RET_NO_DATA) {
        ret = isotp_receive(&tp->func_link, buf, bufsize, &out_size);
        if (ret == ISOTP_RET_OK) {
            UDS_LOGI(__FILE__, "func link received %d bytes", out_size);
            if (NULL != info) {
                info->A_TA = tp->func_sa;
                info->A_SA = tp->func_ta;
                info->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
            }
        } else if (ret == ISOTP_RET_NO_DATA) {
            return 0;
        } else {
            UDS_LOGE(__FILE__, "unhandled return code from func link %d\n", ret);
        }
    } else {
        UDS_LOGE(__FILE__, "unhandled return code from phys link %d\n", ret);
    }
    return out_size;
}

static UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t sa, uint32_t ta, uint32_t sa_func,
                                uint32_t ta_func) {
    if (tp == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    tp->hdl.poll = tp_poll;
    tp->hdl.send = tp_send;
    tp->hdl.recv = tp_recv;
    tp->phys_sa = sa;
    tp->phys_ta = ta;
    tp->func_sa = sa_func;
    tp->func_ta = ta_func;

    isotp_init_link(&tp->phys_link, tp->phys_ta, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
    isotp_init_link(&tp->func_link, tp->func_ta, tp->func_send_buf, sizeof(tp->func_send_buf),
                    tp->func_recv_buf, sizeof(tp->func_recv_buf));
    return UDS_OK;
}

UDSErr_t UDSServerTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t source_addr, uint32_t target_addr,
                               uint32_t source_addr_func) {
    return UDSTpISOTpCInit(tp, source_addr, target_addr, source_addr_func, UDS_TP_NOOP_ADDR);
}

UDSErr_t UDSClientTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t source_addr, uint32_t target_addr,
                               uint32_t target_addr_func) {
    return UDSTpISOTpCInit(tp, source_addr, target_addr, UDS_TP_NOOP_ADDR, target_addr_func);
}

#endif
