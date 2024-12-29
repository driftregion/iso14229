#if defined(UDS_TP_ISOTP_SOCK)

#include "tp/isotp_sock.h"
#include "iso14229.h"
#include <string.h>
#include <errno.h>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static UDSTpStatus_t isotp_sock_tp_poll(UDSTpHandle_t *hdl) { return 0; }

static ssize_t tp_recv_once(int fd, uint8_t *buf, size_t size) {
    ssize_t ret = read(fd, buf, size);
    if (ret < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            ret = 0;
        } else {
            UDS_DBG_PRINT("read failed: %ld with errno: %d\n", ret, errno);
            if (EILSEQ == errno) {
                UDS_DBG_PRINT("Perhaps I received multiple responses?\n");
            }
        }
    }
    return ret;
}

static ssize_t isotp_sock_tp_peek(UDSTpHandle_t *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(p_buf);
    ssize_t ret = 0;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    *p_buf = impl->recv_buf;
    if (impl->recv_len) { // recv not yet acked
        ret = impl->recv_len;
        goto done;
    }

    UDSSDU_t *msg = &impl->recv_info;

    // recv acked, OK to receive
    ret = tp_recv_once(impl->phys_fd, impl->recv_buf, sizeof(impl->recv_buf));
    if (ret > 0) {
        msg->A_TA = impl->phys_sa;
        msg->A_SA = impl->phys_ta;
        msg->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
    } else {
        ret = tp_recv_once(impl->func_fd, impl->recv_buf, sizeof(impl->recv_buf));
        if (ret > 0) {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
            msg->A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL;
        }
    }

    if (ret > 0) {
        fprintf(stdout, "%06d, %s recv, 0x%03x (%s), ", UDSMillis(), impl->tag, msg->A_TA,
                msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        for (unsigned i = 0; i < ret; i++) {
            fprintf(stdout, "%02x ", impl->recv_buf[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout); // flush every time in case of crash
        // UDS_DBG_PRINT("<<< ");
        // UDS_DBG_PRINTHEX(, ret);
    }

done:
    if (ret > 0) {
        impl->recv_len = ret;
        if (info) {
            *info = *msg;
        }
    }
    return ret;
}

static void isotp_sock_tp_ack_recv(UDSTpHandle_t *hdl) {
    UDS_ASSERT(hdl);
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    impl->recv_len = 0;
}

static ssize_t isotp_sock_tp_send(UDSTpHandle_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ssize_t ret = -1;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    int fd;
    const UDSTpAddr_t ta_type = info ? info->A_TA_Type : UDS_A_TA_TYPE_PHYSICAL;

    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        fd = impl->phys_fd;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {
        if (len > 7) {
            UDS_DBG_PRINT("UDSTpIsoTpSock: functional request too large\n");
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
done:
    // UDS_DBG_PRINT(">>> ");
    // UDS_DBG_PRINTHEX(buf, ret);

    fprintf(stdout, "%06d, %s sends, (%s), ", UDSMillis(), impl->tag,
            ta_type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < len; i++) {
        fprintf(stdout, "%02x ", buf[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout); // flush every time in case of crash

    return ret;
}

static ssize_t isotp_sock_tp_get_send_buf(UDSTpHandle_t *hdl, uint8_t **p_buf) {
    UDS_ASSERT(hdl);
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    *p_buf = impl->send_buf;
    return sizeof(impl->send_buf);
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
    // configure socket to wait for tx completion to catch FC frame timeouts
    opts.flags |= CAN_ISOTP_WAIT_TX_DONE;

    if (functional) {
        UDS_DBG_PRINT("configuring fd: %d as functional\n", fd);
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
        UDS_DBG_PRINT("Bind: %s %s\n", strerror(errno), if_name);
        return -1;
    }
    return fd;
}

UDSErr_t UDSTpIsoTpSockInitServer(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func) {
    UDS_ASSERT(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.peek = isotp_sock_tp_peek;
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->hdl.ack_recv = isotp_sock_tp_ack_recv;
    tp->hdl.get_send_buf = isotp_sock_tp_get_send_buf;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, source_addr_func, 0, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        UDS_DBG_PRINT("foo\n");
        fflush(stdout);
        return UDS_FAIL;
    }
    UDS_DBG_PRINT("%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x\n",
                  strlen(tp->tag) ? tp->tag : "server", source_addr, target_addr, source_addr_func,
                  target_addr);
    return UDS_OK;
}

UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func) {
    UDS_ASSERT(tp);
    memset(tp, 0, sizeof(*tp));
    tp->hdl.peek = isotp_sock_tp_peek;
    tp->hdl.send = isotp_sock_tp_send;
    tp->hdl.poll = isotp_sock_tp_poll;
    tp->hdl.ack_recv = isotp_sock_tp_ack_recv;
    tp->hdl.get_send_buf = isotp_sock_tp_get_send_buf;
    tp->func_ta = target_addr_func;
    tp->phys_ta = target_addr;
    tp->phys_sa = source_addr;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, 0, target_addr_func, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        return UDS_FAIL;
    }
    UDS_DBG_PRINT(
        "%s initialized phys link (fd %d) rx 0x%03x tx 0x%03x func link (fd %d) rx 0x%03x tx "
        "0x%03x\n",
        strlen(tp->tag) ? tp->tag : "client", tp->phys_fd, source_addr, target_addr, tp->func_fd,
        source_addr, target_addr_func);
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
