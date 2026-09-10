#if defined(UDS_TP_ISOTP_C_SOCKETCAN)

#include "tp/isotp-c/isotp.h"
#include "tp/isotp_c_private.h"
#include "tp/isotp_c.h"
#include "tp/isotp_c_socketcan.h"
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
    struct sockaddr_can addr = {0};
    struct ifreq ifr = {0};
    int sockfd = -1;

    if ((sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("socket");
        goto done;
    }

    memset(&ifr, 0, sizeof(ifr));
    if (snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname) >= (int)sizeof(ifr.ifr_name)) {
        UDS_LOGE(__FILE__, "Interface name too long");
        close(sockfd);
        sockfd = -1;
        goto done;
    }
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

__attribute__((format(printf, 1, 2))) void isotp_user_debug(const char *message, ...) {
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
}

#ifndef ISO_TP_USER_SEND_CAN_ARG
#error "ISO_TP_USER_SEND_CAN_ARG must be defined"
#endif
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    (void)fflush(stdout);
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

static void SocketCANRecv(UDSTpISOTpCSocketCAN_t *tp) {
    UDS_ASSERT(tp);
    struct can_frame frame = {0};
    ssize_t nbytes = 0;

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
            if (frame.can_id == tp->hdl2.phys_sa) {
                isotp_on_can_message(&tp->hdl2.phys_link, frame.data, frame.can_dlc);
            } else if (frame.can_id == tp->hdl2.func_sa) {
                if (ISOTP_RECEIVE_STATUS_IDLE != tp->hdl2.phys_link.receive_status) {
                    UDS_LOGI(__FILE__,
                             "func frame received but cannot process because link is not idle");
                    return;
                }
                // TODO: reject if it's longer than a single frame
                isotp_on_can_message(&tp->hdl2.func_link, frame.data, frame.can_dlc);
            }
        }
    }
}

static UDSErr_t isotp_c_socketcan_poll(UDSTp_t *hdl) {
    UDSTpISOTpCSocketCAN_t *impl = (UDSTpISOTpCSocketCAN_t *)hdl;
    SocketCANRecv(impl);
    return UDSTpISOTpCPoll((UDSTp_t *)&(impl->hdl2));
}

UDSErr_t UDSTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                  uint32_t source_addr, uint32_t target_addr,
                                  uint32_t source_addr_func, uint32_t target_addr_func) {
    UDSErr_t err = UDS_OK;

    UDSTpISOTpCInit(&tp->hdl2, source_addr, target_addr, source_addr_func, target_addr_func);
    if (err) {
        return err;
    }
    UDSTp_t *hdl = (UDSTp_t *)tp;
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
    close(tp->fd);
    tp->fd = -1;
}

#endif
