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

static UDSTpStatus_t isotp_sock_tp_poll(UDSTp_t *hdl) {
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    UDSTpStatus_t status = 0;
    int ret = 0;
    int fds[2] = {impl->phys_fd, impl->func_fd};
    struct pollfd pfds[2] = {0};
    pfds[0].fd = impl->phys_fd;
    pfds[0].events = POLLERR;
    pfds[0].revents = 0;

    pfds[1].fd = impl->func_fd;
    pfds[1].events = POLLERR;
    pfds[1].revents = 0;

    ret = poll(pfds, 2, 1);
    if (ret < 0) {
        perror("poll");
    } else if (ret == 0) {
        ; // no error
    } else {
        for (int i = 0; i < 2; i++) {
            struct pollfd pfd = pfds[i];
            if (pfd.revents & POLLERR) {
                int pending_err = 0;
                socklen_t len = sizeof(pending_err);
                if (!getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &pending_err, &len) && pending_err) {
                    switch (pending_err) {
                    case ECOMM:
                        UDS_LOGE(__FILE__, "ECOMM: Communication error on send");
                        status |= UDS_TP_ERR;
                        break;
                    default:
                        UDS_LOGE(__FILE__, "Asynchronous socket error: %s (%d)\n",
                                 strerror(pending_err), pending_err);
                        status |= UDS_TP_ERR;
                        break;
                    }
                } else {
                    UDS_LOGE(__FILE__, "POLLERR was set, but no error returned via SO_ERROR?");
                }
            } else {
                UDS_LOGE(__FILE__, "poll() returned, but no POLLERR. revents=0x%x", pfd.revents);
            }
        }
    }
    return status;
}

static ssize_t tp_recv_once(int fd, uint8_t *buf, size_t size) {
    ssize_t ret = read(fd, buf, size);
    if (ret < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            ret = 0;
        } else {
            UDS_LOGI(__FILE__, "read failed: %ld with errno: %d\n", ret, errno);
            if (EILSEQ == errno) {
                UDS_LOGI(__FILE__, "Perhaps I received multiple responses?");
            }
        }
    }
    return ret;
}

static ssize_t isotp_sock_tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(buf);
    ssize_t ret = 0;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    UDSSDU_t *msg = &impl->recv_info;

    ret = tp_recv_once(impl->phys_fd, buf, bufsize);
    if (ret > 0) {
        msg->A_TA = impl->phys_sa;
        msg->A_SA = impl->phys_ta;
        msg->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
    } else {
        ret = tp_recv_once(impl->func_fd, buf, bufsize);
        if (ret > 0) {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
            msg->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
        }
    }

    if (ret > 0) {
        if (info) {
            *info = *msg;
        }

        UDS_LOGD(__FILE__, "'%s' received %ld bytes from 0x%03x (%s), ", impl->tag, ret, msg->A_TA,
                 msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        UDS_LOG_SDU(__FILE__, impl->recv_buf, ret, msg);
    }

    return ret;
}

static ssize_t isotp_sock_tp_send(UDSTp_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ssize_t ret = -1;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    int fd;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        fd = impl->phys_fd;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {
        if (len > 7) {
            UDS_LOGI(__FILE__, "UDSTpIsoTpSock: functional request too large");
            return -1;
        }
        fd = impl->func_fd;
    } else {
        ret = -4;
        goto done;
    }
    ret = write(fd, buf, len);
    if (ret < 0) {
        perror("write");
    }
done:;
    int ta = ta_type == UDS_A_TA_TYPE_PHYSICAL ? impl->phys_ta : impl->func_ta;
    UDS_LOGD(__FILE__, "'%s' sends %ld bytes to 0x%03x (%s)", impl->tag, len, ta,
             ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    UDS_LOG_SDU(__FILE__, buf, len, info);

    return ret;
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
        UDS_LOGI(__FILE__, "configuring fd: %d as functional", fd);
        // configure the socket as listen-only to avoid sending FC frames
        opts.flags |= CAN_ISOTP_LISTEN_MODE;
    }

    if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0) {
        perror("setsockopt (isotp_options):");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
    ioctl(fd, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_addr.tp.rx_id = rxid;
    addr.can_addr.tp.tx_id = txid;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        UDS_LOGI(__FILE__, "Bind: %s %s\n", strerror(errno), if_name);
        return -1;
    }
    return fd;
}

UDSErr_t UDSTpIsoTpSockInitServer(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
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
    tp->func_fd = LinuxSockBind(ifname, source_addr_func, 0, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        UDS_LOGI(__FILE__, "foo\n");
        fflush(stdout);
        return UDS_FAIL;
    }
    const char *tag = "server";
    memmove(tp->tag, tag, strlen(tag));
    UDS_LOGI(__FILE__, "%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x",
             strlen(tp->tag) ? tp->tag : "server", source_addr, target_addr, source_addr_func,
             target_addr);
    return UDS_OK;
}

UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
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
