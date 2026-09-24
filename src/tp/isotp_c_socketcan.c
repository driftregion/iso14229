#if defined(UDS_TP_ISOTP_C_SOCKETCAN)

#include "tp/isotp-c/isotp.h"
#include "tp/isotp_c_private.h"
#include "tp/isotp_c.h"
#include "tp/isotp_c_socketcan.h"
#include "util.h"
#include "util_private.h"
#include "config_internal.h"

static int SetupSocketCAN(const char *ifname) {
    struct sockaddr_can addr = {0};
    struct ifreq ifr = {0};
    int sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);

    if (sockfd < 0) {
        perror("socket");
        goto done;
    }

    (void)memset(&ifr, 0, sizeof(ifr));
    if (snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname) >= (int)sizeof(ifr.ifr_name)) {
        UDS_LOGE(__FILE__, "Interface name too long");
        if (close(sockfd) < 0) {
            perror("close");
        }
        sockfd = -1;
        goto done;
    }
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl");
    }
    (void)memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
    }

done:
    return sockfd;
}

uint32_t isotp_user_get_us(void) { return UDSMillis() * 1000u; }

__attribute__((format(printf, 1, 2))) void isotp_user_debug(const char *message, ...) {
    va_list args;
    va_start(args, message);
    (void)vprintf(message, args);
    va_end(args);
}

#ifndef ISO_TP_USER_SEND_CAN_ARG
#error "ISO_TP_USER_SEND_CAN_ARG must be defined"
#endif
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    (void)fflush(stdout);
    UDS_ASSERT(user_data);
    int sockfd = 0;
    (void)memcpy(&sockfd, user_data, sizeof(sockfd));
    struct can_frame frame = {0};
    frame.can_id = arbitration_id;
    frame.can_dlc = size;
    (void)memmove(frame.data, data, size);
    ssize_t ret = write(sockfd, &frame, sizeof(struct can_frame));
    if (ret < 0) {
        perror("Write err");
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}

static bool GetNextFrame(int fd, struct can_frame *frame) {
    UDS_ASSERT(frame);
    ssize_t nbytes = read(fd, frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        if ((EAGAIN == errno) || (EWOULDBLOCK == errno)) {
            return false;
        } else {
            perror("read");
            return false;
        }
    } else if (nbytes == 0) {
        return false;
    } else {
        return true;
    }
}

static void SocketCANRecv(UDSTpISOTpCSocketCAN_t *tp) {
    UDS_ASSERT(tp);
    struct can_frame frame = {0};

    while (GetNextFrame(tp->fd, &frame)) {
        if (frame.can_id == tp->hdl2.phys_sa) {
            isotp_on_can_message(&tp->hdl2.phys_link, frame.data, frame.can_dlc);
        } else if (frame.can_id == tp->hdl2.func_sa) {
            isotp_on_can_message(&tp->hdl2.func_link, frame.data, frame.can_dlc);
            if (ISOTP_RECEIVE_STATUS_IDLE != tp->hdl2.func_link.receive_status) {
                UDS_LOGI(__FILE__,
                         "func frame received but cannot process because link is not idle");
                break;
            }
            // TODO: reject if it's longer than a single frame
            isotp_on_can_message(&tp->hdl2.func_link, frame.data, frame.can_dlc);
        } else {
            UDS_LOGD(__FILE__, "received frame 0x%x not matching phys or func addresses",
                     frame.can_id);
        }
    }
}

static UDSErr_t isotp_c_socketcan_poll(UDSTp_t *hdl) {
    UDSTpISOTpCSocketCAN_t *impl =
        (UDSTpISOTpCSocketCAN_t *)hdl; // cppcheck-suppress [misra-c2012-11.3]
    SocketCANRecv(impl);
    return UDSTpISOTpCPoll((UDSTp_t *)&(impl->hdl2)); // cppcheck-suppress [misra-c2012-11.3]
}

static UDSErr_t UDSTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                         uint32_t source_addr, uint32_t target_addr,
                                         uint32_t source_addr_func, uint32_t target_addr_func) {
    UDSErr_t err = UDS_OK;

    err = UDSTpISOTpCInit(&tp->hdl2, source_addr, target_addr, source_addr_func, target_addr_func);
    if (UDS_OK != err) {
        return err;
    }
    UDSTp_t *hdl = (UDSTp_t *)tp; // cppcheck-suppress [misra-c2012-11.3]
    hdl->poll = isotp_c_socketcan_poll;

    tp->fd = SetupSocketCAN(ifname);
    tp->hdl2.phys_link.user_send_can_arg = &(tp->fd);
    tp->hdl2.func_link.user_send_can_arg = &(tp->fd);

    return UDS_OK;
}

UDSErr_t UDSServerTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                        uint32_t source_addr, uint32_t target_addr,
                                        uint32_t source_addr_func) {
    return UDSTpISOTpCSocketCANInit(tp, ifname, source_addr, target_addr, source_addr_func,
                                    UDS_TP_NOOP_ADDR);
}

UDSErr_t UDSClientTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                        uint32_t source_addr, uint32_t target_addr,
                                        uint32_t target_addr_func) {
    return UDSTpISOTpCSocketCANInit(tp, ifname, source_addr, target_addr, UDS_TP_NOOP_ADDR,
                                    target_addr_func);
}

void UDSTpISOTpCSocketCANDeinit(UDSTpISOTpCSocketCAN_t *tp) {
    UDS_ASSERT(tp);
    if (close(tp->fd) < 0) {
        perror("close");
    }
    tp->fd = -1;
}

#endif
