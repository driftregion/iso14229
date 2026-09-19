#if defined(UDS_TP_ISOTP_C)

#include "util.h"
#include "log.h"
#include "tp/isotp-c/isotp.h"
#include "tp/isotp_c_shim.h"
#include "tp/isotp_c.h"

UDSErr_t UDSTpISOTpCPoll(UDSTp_t *hdl) {
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    isotp_poll(&impl->phys_link);
    isotp_poll(&impl->func_link);
    if (ISOTP_SEND_STATUS_INPROGRESS == impl->phys_link.send_status) {
        hdl->status.is_sending = 1;
    } else {
        hdl->status.is_sending = 0;
    }

    if (ISOTP_SEND_STATUS_ERROR == impl->phys_link.send_status) {
        return UDS_ERR_TPORT;
    } else {
        return UDS_OK;
    }
}

static UDSErr_t tp_send(UDSTp_t *hdl, const uint8_t *buf, size_t len, const UDSSDU_t *info) {
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    const UDS_A_TA_Type_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    if (len > UINT32_MAX) {
        return UDS_FAIL;
    }

    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &tp->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &tp->func_link;
        if (len > 7) {
            UDS_LOGE(__FILE__, "Cannot send more than 7 bytes via functional addressing");
            return UDS_ERR_MISUSE;
        }
        break;
    default:
        UDS_LOGE(__FILE__, "unknown UDS_A_TA_TYPE");
        return UDS_ERR_MISUSE;
    }

    int ret = isotp_send(link, buf, (uint32_t)len);
    switch (ret) {
    case ISOTP_RET_OK: {
        return UDS_OK;
    }
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        return UDS_ERR_TPORT;
    }
}

static inline UDSErr_t
safe_api_shim_isotp_receive(IsoTpLink *link, uint8_t *payload,
                            const size_t payload_size, // size of payload buffer
                            size_t *out_size, int *isotp_ret) {
    uint32_t u32out_size = 0;

    if (payload_size > UINT32_MAX) { // max value of isotp_receive(payload_size)
        return UDS_FAIL;
    }

    *isotp_ret = isotp_receive(link, payload, (uint32_t)payload_size, &u32out_size);

#if UINT32_MAX > SIZE_MAX
    if (u32out_size > SIZE_MAX) {
        return UDS_FAIL;
    }
#endif

    *out_size = u32out_size;
    return UDS_OK;
}

static UDSErr_t tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsiz, size_t *recvlen,
                        UDSSDU_t *info) {
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    int ret = 0;
    UDSErr_t err = UDS_OK;

    err = safe_api_shim_isotp_receive(&tp->phys_link, buf, bufsiz, recvlen, &ret);
    if (UDS_OK != err) {
        goto done;
    }
    if (ISOTP_RET_OK == ret) {
        UDS_LOGI(__FILE__, "phys link received %zd bytes", *recvlen);
        if (NULL != info) {
            info->A_TA = tp->phys_sa;
            info->A_SA = tp->phys_ta;
            info->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
        }
    } else if (ISOTP_RET_NO_DATA == ret) {
        err = safe_api_shim_isotp_receive(&tp->func_link, buf, bufsiz, recvlen, &ret);
        if (UDS_OK != err) {
            goto done;
        }
        if (ISOTP_RET_OK == ret) {
            UDS_LOGI(__FILE__, "func link received %zd bytes", *recvlen);
            if (NULL != info) {
                info->A_TA = tp->func_sa;
                info->A_SA = tp->func_ta;
                info->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
            }
        } else if (ISOTP_RET_NO_DATA == ret) {
            goto done;
        } else {
            UDS_LOGE(__FILE__, "unhandled return code from func link %d\n", ret);
        }
    } else {
        UDS_LOGE(__FILE__, "unhandled return code from phys link %d\n", ret);
    }
done:
    return err;
}

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t sa, uint32_t ta, uint32_t sa_func,
                         uint32_t ta_func) {
    if (tp == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    tp->hdl.poll = UDSTpISOTpCPoll;
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
