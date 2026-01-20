#if defined(UDS_TP_DOIP)

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>

#include "doip_transport.h"
#include "doip_defines.h"

int doip_tp_tcp_init(DoIPTransport *t, const char *ip, uint16_t port) {
    if (!t || !ip) return -1;
    memset(t, 0, sizeof(*t));
    t->fd = -1;
    t->is_udp = false;
    t->port = port ? port : DOIP_TCP_PORT;
    snprintf(t->ip, sizeof(t->ip), "%s", ip);
    t->connect_timeout_ms = DOIP_DEFAULT_TIMEOUT_MS;
    t->send_timeout_ms = DOIP_DEFAULT_TIMEOUT_MS;
    return 0;
}

int doip_tp_tcp_connect(DoIPTransport *t) {
    if (!t) return -1;
    t->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (t->fd < 0) {
        return -1;
    }

    /* set non-blocking before connect to avoid blocking connect */
    int flags = fcntl(t->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(t->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(t->fd);
        t->fd = -1;
        return -1;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(t->port);
    if (inet_pton(AF_INET, t->ip, &sa.sin_addr) <= 0) {
        close(t->fd);
        t->fd = -1;
        return -1;
    }
    int rc = connect(t->fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0) {
        if (errno != EINPROGRESS) {
            close(t->fd);
            t->fd = -1;
            return -1;
        }
        /* wait for writability or error within default timeout */
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(t->fd, &wfds);
        struct timeval tv;
        int cto = (t->connect_timeout_ms > 0) ? t->connect_timeout_ms : DOIP_DEFAULT_TIMEOUT_MS;
        tv.tv_sec = cto / 1000;
        tv.tv_usec = (cto % 1000) * 1000;
        rc = select(t->fd + 1, NULL, &wfds, NULL, &tv);
        if (rc <= 0) {
            /* timeout or select error */
            close(t->fd);
            t->fd = -1;
            return -1;
        }
        int soerr = 0;
        socklen_t slen = sizeof(soerr);
        if (getsockopt(t->fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 || soerr != 0) {
            close(t->fd);
            t->fd = -1;
            return -1;
        }
    }
    return 0;
}

ssize_t doip_tp_tcp_send(DoIPTransport *t, const uint8_t *buf, size_t len) {
    if (!t || t->fd < 0 || !buf) return -1;
    size_t total = 0;
    int sflags = 0;
#ifdef MSG_NOSIGNAL
    sflags |= MSG_NOSIGNAL;
#endif
    while (total < len) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(t->fd, &wfds);
        struct timeval tv;
        int sto = (t->send_timeout_ms > 0) ? t->send_timeout_ms : DOIP_DEFAULT_TIMEOUT_MS;
        tv.tv_sec = sto / 1000;
        tv.tv_usec = (sto % 1000) * 1000;
        int rc = select(t->fd + 1, NULL, &wfds, NULL, &tv);
        if (rc <= 0) {
            /* timeout or error */
            return -1;
        }
        ssize_t n = send(t->fd, buf + total, len - total, sflags);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            /* try again after select */
            continue;
        }
        /* other error or peer closed */
        return -1;
    }
    return (ssize_t)total;
}

void doip_tp_set_timeouts(DoIPTransport *t, int connect_timeout_ms, int send_timeout_ms) {
    if (!t) return;
    if (connect_timeout_ms > 0) t->connect_timeout_ms = connect_timeout_ms;
    if (send_timeout_ms > 0) t->send_timeout_ms = send_timeout_ms;
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
