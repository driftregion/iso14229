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

static UDSTpStatus_t tp_poll(UDSTpHandle_t *hdl) { return 0; }

static ssize_t tp_recv_once(int fd, UDSSDU_t *msg, bool functional) {
    ssize_t ret = read(fd, msg->A_Data, msg->A_DataBufSize);
    if (ret < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            ret = 0;
        } else {
            UDS_DBG_PRINT("read failed: %ld with errno: %d\n", ret, errno);
            if (EILSEQ == errno) {
                UDS_DBG_PRINT("Perhaps I received multiple responses?\n");
            }
        }
    } else if (ret > 0) {
        msg->A_Length = ret;
        msg->A_TA_Type = functional ? UDS_A_TA_TYPE_FUNCTIONAL : UDS_A_TA_TYPE_PHYSICAL;
    }
    return ret;
}

static ssize_t tp_recv(UDSTpHandle_t *hdl, UDSSDU_t *msg) {
    assert(hdl);
    assert(msg);
    ssize_t ret = 0;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;

    ret = tp_recv_once(impl->phys_fd, msg, false);
    if (ret > 0) {
        msg->A_TA = impl->phys_sa;
        msg->A_SA = impl->phys_ta;
    } else {
        ret = tp_recv_once(impl->func_fd, msg, true);
        if (ret > 0) {
            msg->A_TA = impl->func_sa;
            msg->A_SA = impl->func_ta;
        }
    }

    if (ret > 0) {
        fprintf(stdout, "%06d, %s recv, 0x%03x (%s), ", UDSMillis(), impl->tag, msg->A_TA,
                msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
        for (unsigned i = 0; i < msg->A_Length; i++) {
            fprintf(stdout, "%02x ", msg->A_Data[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout); // flush every time in case of crash
        // UDS_DBG_PRINT("<<< ");
        // UDS_DBG_PRINTHEX(, ret);
    }
    return ret;
}

static ssize_t tp_send(UDSTpHandle_t *hdl, UDSSDU_t *msg) {
    assert(hdl);
    ssize_t ret = -1;
    UDSTpIsoTpSock_t *impl = (UDSTpIsoTpSock_t *)hdl;
    int fd;
    switch (msg->A_TA_Type) {
    case kTpAddrTypePhysical:
        fd = impl->phys_fd;
        break;
    case kTpAddrTypeFunctional:
        fd = impl->func_fd;
        break;
    default:
        ret = -4;
        goto done;
    }
    ret = write(fd, msg->A_Data, msg->A_Length);
    if (ret < 0) {
        perror("write");
    }
done:
    // UDS_DBG_PRINT(">>> ");
    // UDS_DBG_PRINTHEX(buf, ret);
    fprintf(stdout, "%06d, %s sends, 0x%03x (%s), ", UDSMillis(), impl->tag, msg->A_TA,
            msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < msg->A_Length; i++) {
        fprintf(stdout, "%02x ", msg->A_Data[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout); // flush every time in case of crash
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

    if (functional) {
        printf("configuring fd: %d as functional\n", fd);
        // configure the socket as listen-only to avoid sending FC frames
        struct can_isotp_options opts;
        memset(&opts, 0, sizeof(opts));
        opts.flags |= CAN_ISOTP_LISTEN_MODE;
        if (setsockopt(fd, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0) {
            perror("setsockopt (isotp_options):");
            return -1;
        }
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
        printf("Bind: %s %s\n", strerror(errno), if_name);
        return -1;
    }
    return fd;
}

UDSErr_t UDSTpIsoTpSockInitServer(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func) {
    assert(tp);
    tp->hdl.recv = tp_recv;
    tp->hdl.send = tp_send;
    tp->hdl.poll = tp_poll;
    tp->phys_sa = source_addr;
    tp->phys_ta = target_addr;
    tp->func_sa = source_addr_func;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, source_addr_func, target_addr, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        return UDS_ERR;
    }
    printf("%s initialized phys link rx 0x%03x tx 0x%03x func link rx 0x%03x tx 0x%03x\n",
           tp->tag ? tp->tag : "server", source_addr, target_addr, source_addr_func, target_addr);
    return UDS_OK;
}

UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func) {
    assert(tp);
    tp->hdl.recv = tp_recv;
    tp->hdl.send = tp_send;
    tp->hdl.poll = tp_poll;
    tp->func_ta = target_addr_func;
    tp->phys_ta = target_addr;
    tp->phys_sa = source_addr;

    tp->phys_fd = LinuxSockBind(ifname, source_addr, target_addr, false);
    tp->func_fd = LinuxSockBind(ifname, source_addr + 1, target_addr_func, true);
    if (tp->phys_fd < 0 || tp->func_fd < 0) {
        return UDS_ERR;
    }
    printf("%s initialized phys link (fd %d) rx 0x%03x tx 0x%03x func link (fd %d) rx 0x%03x tx 0x%03x\n",
           tp->tag ? tp->tag : "client", tp->phys_fd, source_addr, target_addr, tp->func_fd, source_addr, target_addr_func);
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
