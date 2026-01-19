#if defined(UDS_TP_DOIP)

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "doip_transport.h"
#include "doip_defines.h"

int doip_tp_tcp_init(DoIPTransport *t, const char *ip, uint16_t port) {
    if (!t || !ip) return -1;
    memset(t, 0, sizeof(*t));
    t->fd = -1;
    t->is_udp = false;
    t->port = port ? port : DOIP_TCP_PORT;
    snprintf(t->ip, sizeof(t->ip), "%s", ip);
    return 0;
}

int doip_tp_tcp_connect(DoIPTransport *t) {
    if (!t) return -1;
    t->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (t->fd < 0) {
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = DOIP_DEFAULT_TIMEOUT_MS / 1000;
    tv.tv_usec = (DOIP_DEFAULT_TIMEOUT_MS % 1000) * 1000;
    (void)setsockopt(t->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(t->port);
    if (inet_pton(AF_INET, t->ip, &sa.sin_addr) <= 0) {
        close(t->fd);
        t->fd = -1;
        return -1;
    }
    if (connect(t->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(t->fd);
        t->fd = -1;
        return -1;
    }
    return 0;
}

ssize_t doip_tp_tcp_send(DoIPTransport *t, const uint8_t *buf, size_t len) {
    if (!t || t->fd < 0 || !buf) return -1;
    return send(t->fd, buf, len, 0);
}

ssize_t doip_tp_tcp_recv(DoIPTransport *t, uint8_t *buf, size_t len, int timeout_ms) {
    if (!t || t->fd < 0 || !buf) return -1;

    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(t->fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(t->fd + 1, &rfds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
    if (ret < 0) {
        return -1;
    }
    if (ret == 0) {
        return 0; /* timeout */
    }
    return recv(t->fd, buf, len, 0);
}

void doip_tp_tcp_close(DoIPTransport *t) {
    if (!t) return;
    if (t->fd >= 0) {
        close(t->fd);
        t->fd = -1;
    }
}

#endif /* UDS_TP_DOIP */
