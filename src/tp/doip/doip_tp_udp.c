#if defined(UDS_TP_DOIP)

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "doip_transport.h"

/* Default DoIP multicast group and port for discovery */
static const char *DOIP_DEFAULT_MCAST = "224.224.224.224"; /* per ISO 13400 */
static const uint16_t DOIP_DEFAULT_UDP_PORT = 13400;

int doip_tp_udp_init(DoIPTransport *t, uint16_t port, bool loopback) {
    if (!t) return -1;
    memset(t, 0, sizeof(*t));
    t->fd = -1;
    t->is_udp = true;
    t->loopback = loopback;
    t->port = port ? port : DOIP_DEFAULT_UDP_PORT;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    int reuse = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (loopback) {
        /* Bind to loopback UDP to allow local discovery testing */
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(t->port);
        sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            close(fd);
            return -1;
        }

        unsigned char on = 1;
        (void)setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &on, sizeof(on));
    } else {
        /* Bind on any address for multicast */
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(t->port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            close(fd);
            return -1;
        }
    }

    t->fd = fd;
    return 0;
}

int doip_tp_udp_join_default_multicast(DoIPTransport *t) {
    if (!t || t->fd < 0) return -1;
    if (t->loopback) return 0; /* no multicast join needed */

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(DOIP_DEFAULT_MCAST);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(t->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return -1;
    }
    return 0;
}

ssize_t doip_tp_udp_recv(DoIPTransport *t, uint8_t *buf, size_t len, int timeout_ms) {
    if (!t || t->fd < 0 || !buf) return -1;
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(t->fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(t->fd + 1, &rfds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
    if (ret < 0) return -1;
    if (ret == 0) return 0; /* timeout */

    return recv(t->fd, buf, len, 0);
}

void doip_tp_udp_close(DoIPTransport *t) {
    if (!t) return;
    if (t->fd >= 0) {
        close(t->fd);
        t->fd = -1;
    }
}

#endif /* UDS_TP_DOIP */
