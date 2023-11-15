#include "tp/isotp_c_socketcan.h"
#include "iso14229.h"
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

static int sockfd = 0;
static bool HasSetup = false;



static void SetupOnce() {
    if (HasSetup) {
        return;
    }
    UDS_DBG_PRINT("setting up CAN\n");
    struct sockaddr_can addr;
    struct ifreq ifr;
    if ((sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("socket");
        exit(-1);
    }

    // TODO: https://github.com/lishen2/isotp-c/issues/14
    int recv_own_msgs = 1;
    if (setsockopt(sockfd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own_msgs, sizeof(recv_own_msgs)) < 0) {
        perror("setsockopt (CAN_RAW_LOOPBACK):");
        exit(-1);
    }


    strcpy(ifr.ifr_name, "vcan0");
    ioctl(sockfd, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(-1);
    }
    HasSetup = true;
}

uint32_t isotp_user_get_us() { return UDSMillis() * 1000; }

void isotp_user_debug(const char *message, ...) {}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    SetupOnce();
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

static void printhex(const uint8_t *addr, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x,", addr[i]);
    }
    printf("\n");
}

static void SocketCANRecv(UDSTpISOTpC_t *tp) {
    assert(tp);
    struct can_frame frame = {0};
    int nbytes = 0;
    for (;;) {
        nbytes = read(sockfd, &frame, sizeof(struct can_frame));
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
                UDS_DBG_PRINT("func recvd can\n");
                UDS_DBG_PRINTHEX(frame.data, frame.can_dlc);
                isotp_on_can_message(&tp->func_link, frame.data, frame.can_dlc);
            }
        }
    }
}

static UDSTpStatus_t tp_poll(UDSTpHandle_t *hdl) {
    assert(hdl);
    UDSTpStatus_t status = 0;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    SocketCANRecv(impl);

    isotp_poll(&impl->phys_link);
    isotp_poll(&impl->func_link);
    if (impl->phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
        status |= UDS_TP_SEND_IN_PROGRESS;
    }
    return status;
}

static ssize_t tp_recv(UDSTpHandle_t *hdl, UDSSDU_t *msg) {
    assert(hdl);
    int ret = -1;
    uint16_t size = 0;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    struct {
        IsoTpLink *link;
        UDSTpAddr_t ta_type;
    } arr[] = {{&impl->phys_link, UDS_A_TA_TYPE_PHYSICAL}, {&impl->func_link, UDS_A_TA_TYPE_FUNCTIONAL}};
    for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
        ret = isotp_receive(arr[i].link, msg->A_Data, msg->A_DataBufSize, &size);
        switch (ret) {
        case ISOTP_RET_OK:
            msg->A_TA_Type = arr[i].ta_type;
            ret = size;
            goto done;
        case ISOTP_RET_NO_DATA:
            ret = 0;
            continue;
        case ISOTP_RET_ERROR:
            ret = -1;
            goto done;
        default:
            ret = -2;
            goto done;
        }
    }
done:
    if (ret > 0) {
        msg->A_Length = size;
        if (UDS_A_TA_TYPE_PHYSICAL == msg->A_TA_Type )  {
            msg->A_TA = impl->phys_sa;
            msg->A_SA = impl->phys_ta;
        } else {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
        }
        fprintf(stdout, "%06d, %s recv, 0x%03x (%s), ", UDSMillis(), impl->tag, msg->A_TA,
                msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        for (unsigned i = 0; i < msg->A_Length; i++) {
            fprintf(stdout, "%02x ", msg->A_Data[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout); // flush every time in case of crash

    }
    return ret;
}

static ssize_t tp_send(UDSTpHandle_t *hdl, uint8_t *buf, ssize_t len, UDSSDU_t *info) {
    assert(hdl);
    ssize_t ret = -1;
    UDSTpISOTpC_t *impl = (UDSTpISOTpC_t *)hdl;
    IsoTpLink *link = NULL;
    switch (msg->A_TA_Type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        link = &impl->phys_link;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL:
        link = &impl->func_link;
        break;
    default:
        ret = -4;
        goto done;
    }

    int send_status = isotp_send(link, msg->A_Data, msg->A_Length);
    switch (send_status) {
    case ISOTP_RET_OK:
        ret = msg->A_Length;
        goto done;
    case ISOTP_RET_INPROGRESS:
    case ISOTP_RET_OVERFLOW:
    default:
        ret = send_status;
        goto done;
    }
done:
    fprintf(stdout, "%06d, %s sends, 0x%03x (%s), ", UDSMillis(), impl->tag, msg->A_TA,
            msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < msg->A_Length; i++) {
        fprintf(stdout, "%02x ", msg->A_Data[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout); // flush every time in case of crash

    return ret;
}


UDSErr_t UDSTpISOTpCInitServer(UDSTpISOTpC_t *tp, UDSServer_t* srv, const char *ifname, uint32_t source_addr,
                               uint32_t target_addr, uint32_t target_addr_func) {
    assert(tp);
    assert(ifname);
    tp->hdl.poll = tp_poll;
    tp->hdl.recv = tp_recv;
    tp->hdl.send = tp_send;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr;
    tp->func_ta = target_addr_func;

    isotp_init_link(&tp->phys_link, target_addr, srv->send_buf, sizeof(srv->send_buf),
                    srv->recv_buf, sizeof(srv->recv_buf));
    isotp_init_link(&tp->func_link, target_addr, tp->func_send_buf, sizeof(tp->func_send_buf),
                    tp->func_recv_buf, sizeof(tp->func_recv_buf));
    return UDS_OK;
}

UDSErr_t UDSTpISOTpCInitClient(UDSTpISOTpC_t *tp, UDSClient_t *client, const char *ifname, uint32_t source_addr, uint32_t target_addr, uint32_t source_addr_func) {
    assert(tp);
    assert(ifname);
    tp->hdl.poll = tp_poll;
    tp->hdl.recv = tp_recv;
    tp->hdl.send = tp_send;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;
    tp->func_ta = target_addr;

    isotp_init_link(&tp->phys_link, target_addr, client->send_buf, sizeof(client->send_buf),
                    client->recv_buf, sizeof(client->recv_buf));
    isotp_init_link(&tp->func_link, target_addr, tp->func_send_buf, sizeof(tp->func_send_buf),
                    tp->func_recv_buf, sizeof(tp->func_recv_buf));
    return UDS_OK;
}
 