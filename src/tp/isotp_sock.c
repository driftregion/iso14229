#if defined(UDS_TP_ISOTP_SOCK)

#include "tp/isotp_sock.h"
#include "uds.h"
#include "log.h"
#include <string.h>
#include <errno.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static UDSErr_t isotp_sock_tp_poll(UDSTp_t *hdl) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    UDSErr_t err = UDS_OK;
    int ret = 0;
    int fds[2] = {impl->phys_fd, impl->func_fd};
    struct pollfd pfds[2] = {0};
    pfds[0].fd = impl->phys_fd;
    pfds[0].events = POLLERR | POLLOUT;
    pfds[0].revents = 0;

    pfds[1].fd = impl->func_fd;
    pfds[1].events = POLLERR | POLLOUT;
    pfds[1].revents = 0;

    ret = poll(pfds, 2, 1);
    if (ret < 0) {
        UDS_LOGE(__FILE__, "poll failed: %d", ret);
        err = UDS_ERR_TPORT;
    } else if (ret == 0) {
        ; // timeout, no events
    } else {
        // poll() returned with events
        for (int i = 0; i < 2; i++) {
            struct pollfd pfd = pfds[i];

            // Check for errors
            if (pfd.revents & POLLERR) {
                int pending_err = 0;
                socklen_t len = sizeof(pending_err);
                if (!getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &pending_err, &len) && pending_err) {
                    switch (pending_err) {
                    case ECOMM:
                        UDS_LOGE(__FILE__, "ECOMM: Communication error on send");
                        err = UDS_ERR_TPORT;
                        break;
                    default:
                        UDS_LOGE(__FILE__, "Asynchronous socket error: %s (%d)",
                                 strerror(pending_err), pending_err);
                        break;
                    }
                } else {
                    UDS_LOGE(__FILE__, "POLLERR was set, but no error returned via SO_ERROR?");
                }
            }

            // Check if send is in progress on physical socket
            // Only check the physical socket (not functional) since that's what sends multi-frame
            if (fds[i] == impl->phys_fd && pfd.revents != 0) {
                // When POLLOUT is NOT set but other events are present, the socket cannot accept
                // writes because a multi-frame transmission is in progress.
                // See: https://lore.kernel.org/all/20230331125511.372783-1-michal.sojka@cvut.cz/
                // The kernel ISO-TP driver suppresses POLLOUT when tx.state != ISOTP_IDLE
                if (!(pfd.revents & POLLOUT)) {
                    hdl->status.is_sending = 1;
                } else {
                    hdl->status.is_sending = 0;
                }
            }
        }
    }
    return err;
}

static UDSErr_t tp_recv_once(int fd, uint8_t *buf, const size_t bufsiz, size_t *recvlen) {
    UDSErr_t err = UDS_OK;
    ssize_t ret = read(fd, buf, bufsiz);
    if (ret < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            ; // temporarily unavailable -- not an error
        } else {
            UDS_LOGE(__FILE__, "read failed: %zd with errno: %d", ret, errno);
            err = UDS_FAIL;
        }
    }

    *recvlen = ret < 0 ? 0 : (size_t)ret;
    return err;
}

static UDSErr_t isotp_sock_tp_recv(UDSTp_t *hdl, uint8_t *buf, const size_t bufsiz, size_t *recvlen,
                                   UDSSDU_t *info) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    UDSErr_t err = 0;
    UDSSDU_t *msg = &impl->recv_info;

    err = tp_recv_once(impl->phys_fd, buf, bufsiz, recvlen);
    if (err) {
        return err;
    }
    if (*recvlen > 0) {
        msg->A_TA = impl->phys_sa;
        msg->A_SA = impl->phys_ta;
        msg->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
    } else {
        err = tp_recv_once(impl->func_fd, buf, bufsiz, recvlen);
        if (err) {
            return err;
        }
        if (*recvlen > 0) {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
            msg->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
        }
    }

    if (*recvlen > 0) {
        if (info) {
            *info = *msg;
        }

        UDS_LOGD(__FILE__, "'%s' received %zd bytes from 0x%03x (%s), ", impl->tag, *recvlen,
                 msg->A_TA, msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        UDS_LOG_SDU(__FILE__, impl->recv_buf, *recvlen, msg);
    }
    return UDS_OK;
}

static UDSErr_t isotp_sock_tp_send(UDSTp_t *hdl, const uint8_t *buf, const size_t len,
                                   const UDSSDU_t *info) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    ssize_t ret = -1;
    int fd = -1;
    const UDS_A_TA_Type_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    switch (ta_type) {
    case UDS_A_TA_TYPE_PHYSICAL:
        fd = impl->phys_fd;
        break;
    case UDS_A_TA_TYPE_FUNCTIONAL: {
        if (len > 7) {
            UDS_LOGE(__FILE__, "UDSTpIsoTpSock: functional request too large");
            return UDS_ERR_MISUSE;
        }
        fd = impl->func_fd;
    } break;
    default:
        UDS_LOGE(__FILE__, "unknown UDS_A_TA_TYPE");
        return UDS_ERR_MISUSE;
    }

    UDS_ASSERT(fd >= 0);
    ret = write(fd, buf, len);
    UDS_ASSERT(ret < UINT16_MAX);
    if (ret < 0) {
        perror("write");
        return UDS_FAIL;
    }

    uint32_t ta = ta_type == UDS_A_TA_TYPE_PHYSICAL ? impl->phys_ta : impl->func_ta;
    UDS_LOGD(__FILE__, "'%s' sends %zu bytes to 0x%03x (%s)", impl->tag, len, ta,
             ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    UDS_LOG_SDU(__FILE__, buf, len, info);
    return UDS_OK;
}

static int LinuxSockBind(const char *if_name, uint32_t rxid, uint32_t txid, bool functional) {
    int fd = 0;
    if ((fd = socket(AF_CAN, SOCK_DGRAM | SOCK_NONBLOCK, CAN_ISOTP)) < 0) {
        perror("Socket");
        return -1;
    }

    struct can_isotp_fc_options fcopts = {
        .bs = 0x10,
        .stmin = 3,
        .wftmax = 0,
    };
    if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_RECV_FC, &fcopts, sizeof(fcopts)) < 0) {
        perror("setsockopt");
        return -1;
    }

    struct can_isotp_options opts;
    memset(&opts, 0, sizeof(opts));

    if (functional) {
        // configure the socket as listen-only to avoid sending FC frames
        opts.flags |= CAN_ISOTP_LISTEN_MODE;
    }

    if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0) {
        perror("setsockopt (isotp_options):");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    if (snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", if_name) >= (int)sizeof(ifr.ifr_name)) {
        UDS_LOGE(__FILE__, "Interface name too long");
        close(fd);
        return -1;
    }
    ioctl(fd, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_addr.tp.rx_id = rxid;
    addr.can_addr.tp.tx_id = txid;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        UDS_LOGI(__FILE__, "Bind: %s %s", strerror(errno), if_name);
        return -1;
    }
    return fd;
}

UDSErr_t UDSServerTpIsoTpSockInit(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func) {
    UDS_ASSERT(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.recv = isotp_sock_tp_recv;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    if (tp->phys_fd < 0) {
        return UDS_FAIL;
    }
    tp->func_fd = LinuxSockBind(ifname, source_addr_func, 0, true);
    if (tp->func_fd < 0) {
        return UDS_FAIL;
    }
    const char *tag = "server";
    memmove(tp->tag, tag, strlen(tag));
    UDS_LOGI(__FILE__, "%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x",
             strlen(tp->tag) ? tp->tag : "server", source_addr, target_addr, source_addr_func,
             target_addr);
    return UDS_OK;
}

UDSErr_t UDSClientTpIsoTpSockInit(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func) {
    UDS_ASSERT(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.recv = isotp_sock_tp_recv;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->func_ta = target_addr_func;
    tp->phys_ta = target_addr;
    tp->phys_sa = source_addr;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, 0, target_addr_func, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        return UDS_FAIL;
    }
    const char *tag = "client";
    memmove(tp->tag, tag, strlen(tag));
    UDS_LOGI(__FILE__,
             "%s initialized phys link (fd %d) rx 0x%03x tx 0x%03x func link (fd %d) rx 0x%03x tx "
             "0x%03x",
             strlen(tp->tag) ? tp->tag : "client", tp->phys_fd, source_addr, target_addr,
             tp->func_fd, source_addr, target_addr_func);
    return UDS_OK;
}

void UDSTpIsoTpSockDeinit(UDSTpIsoTpSock_t *tp) {
    if (tp) {
        if (close(tp->phys_fd) < 0) {
            perror("failed to close socket");
        }
        if (close(tp->func_fd) < 0) {
            perror("failed to close socket");
        }
    }
}

#endif
