#if defined(UDS_TP_ISOTP_C_SOCKETCAN)

#include "tp/isotp_c_socketcan.h"
#include "iso14229.h"
#include "tp/isotp-c/isotp_defines.h"
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

static int SetupSocketCAN(const char *ifname) {
    UDS_DBG_PRINT("setting up CAN\n");
    struct sockaddr_can addr;
    struct ifreq ifr;
    int sockfd = -1;

    if ((sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("socket");
        goto done;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(sockfd, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
    }

done:
    return sockfd;
}

uint32_t isotp_user_get_us(void) { return UDSMillis() * 1000; }

void isotp_user_debug(const char *message, ...) {
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    printf("user_data: %p\n", user_data);
    fflush(stdout);
    UDS_ASSERT(user_data);
    int sockfd = *(int *)user_data;
    struct can_frame frame = {0};
    frame.can_id = arbitration_id;
    frame.can_dlc = size;
    memmove(frame.data, data, size);
    if (write(sockfd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write err");
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}

static void SocketCANRecv(UDSTpISOTpC_t *tp) {
    UDS_ASSERT(tp);
    struct can_frame frame = {0};
    int nbytes = 0;

    for (;;) {
        nbytes = read(tp->fd, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                break;
            } else {
                perror("read");
            }
        } else if (nbytes == 0) {
            break;
        } else {
            if (frame.can_id == tp->phys_sa) {
                UDS_DBG_PRINT("phys recvd can\n");
                UDS_DBG_PRINTHEX(frame.data, frame.can_dlc);
                isotp_on_can_message(&tp->phys_link, frame.data, frame.can_dlc);
            } else if (frame.can_id == tp->func_sa) {
                if (ISOTP_RECEIVE_STATUS_IDLE != tp->phys_link.receive_status) {
                    UDS_DBG_PRINT(
                        "func frame received but cannot process because link is not idle");
                    return;
                }
                // TODO: reject if it's longer than a single frame
                isotp_on_can_message(&tp->func_link, frame.data, frame.can_dlc);
            }
        }
    }
}

static UDSTpStatus_t isotp_c_socketcan_tp_poll(UDSTpHandle_t *hdl) {
    UDS_ASSERT(hdl);
    UDSTpStatus_t status = 0;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    SocketCANRecv(impl);
    isotp_poll(&impl->phys_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

static int isotp_c_socketcan_tp_peek_link(IsoTpLink *link, uint8_t *buf, size_t bufsize,
                                          bool functional) {
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
        UDS_DBG_PRINT("The link is full. Copying %d bytes\n", ret);
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

static ssize_t isotp_c_socketcan_tp_peek(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(p_buf);
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    if (ISOTP_RECEIVE_STATUS_FULL == tp->phys_link.receive_status) { // recv not yet acked
        *p_buf = tp->recv_buf;
        return tp->phys_link.receive_size;
    }
    int ret = -1;
    ret = isotp_c_socketcan_tp_peek_link(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), false);
    UDS_A_TA_Type_t ta_type = UDS_A_TA_TYPE_PHYSICAL;
    uint32_t ta = tp->phys_ta;
    uint32_t sa = tp->phys_sa;

    if (ret > 0) {
        UDS_DBG_PRINT("just got %d bytes\n", ret);
        ta = tp->phys_sa;
        sa = tp->phys_ta;
        ta_type = UDS_A_TA_TYPE_PHYSICAL;
        *p_buf = tp->recv_buf;
        goto done;
    } else if (ret < 0) {
        goto done;
    } else {
        ret = isotp_c_socketcan_tp_peek_link(&tp->func_link, tp->recv_buf, sizeof(tp->recv_buf),
                                             true);
        if (ret > 0) {
            UDS_DBG_PRINT("just got %d bytes on func link \n", ret);
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
        fprintf(stdout, "%06d, %s recv, 0x%03x (%s), ", UDSMillis(), tp->tag, ta,
                ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        for (int i = 0; i < ret; i++) {
            fprintf(stdout, "%02x ", (*p_buf)[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout); // flush every time in case of crash
    }
    return ret;
}

static ssize_t isotp_c_socketcan_tp_send(UDSTpHandle_t *hdl, uint8_t *buf, size_t len,
                                         UDSSDU_t *info) {
    UDS_ASSERT(hdl);
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
    fprintf(stdout, "%06d, %s sends, 0x%03x (%s), ", UDSMillis(), tp->tag, ta,
            ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < len; i++) {
        fprintf(stdout, "%02x ", buf[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout); // flush every time in case of crash

    return ret;
}

static void isotp_c_socketcan_tp_ack_recv(UDSTpHandle_t *hdl) {
    UDS_DBG_PRINT("ack recv\n");
    UDS_ASSERT(hdl);
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    uint16_t out_size = 0;
    isotp_receive(&tp->phys_link, tp->recv_buf, sizeof(tp->recv_buf), &out_size);
}

static ssize_t isotp_c_socketcan_tp_get_send_buf(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    UDS_ASSERT(hdl);
    UDSTpISOTpC_t *tp = (UDSTpISOTpC_t *)hdl;
    *p_buf = tp->send_buf;
    return sizeof(tp->send_buf);
}

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, const char *ifname, uint32_t source_addr,
                         uint32_t target_addr, uint32_t source_addr_func,
                         uint32_t target_addr_func) {
    UDS_ASSERT(tp);
    UDS_ASSERT(ifname);
    tp->hdl.poll = isotp_c_socketcan_tp_poll;
    tp->hdl.send = isotp_c_socketcan_tp_send;
    tp->hdl.peek = isotp_c_socketcan_tp_peek;
    tp->hdl.ack_recv = isotp_c_socketcan_tp_ack_recv;
    tp->hdl.get_send_buf = isotp_c_socketcan_tp_get_send_buf;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;
    tp->func_ta = target_addr;
    tp->fd = SetupSocketCAN(ifname);

    isotp_init_link(&tp->phys_link, target_addr, tp->send_buf, sizeof(tp->send_buf), tp->recv_buf,
                    sizeof(tp->recv_buf));
    isotp_init_link(&tp->func_link, target_addr_func, tp->recv_buf, sizeof(tp->send_buf),
                    tp->recv_buf, sizeof(tp->recv_buf));

    tp->phys_link.user_send_can_arg = &(tp->fd);
    tp->func_link.user_send_can_arg = &(tp->fd);

    return UDS_OK;
}

void UDSTpISOTpCDeinit(UDSTpISOTpC_t *tp) {
    UDS_ASSERT(tp);
    close(tp->fd);
    tp->fd = -1;
}

#endif
