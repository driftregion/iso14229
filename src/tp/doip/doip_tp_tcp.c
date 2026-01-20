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

void doip_tp_set_timeouts(DoIPTcpTransport *tcp, int connect_timeout_ms, int send_timeout_ms) {
    if (!tcp) return;
    if (connect_timeout_ms > 0) tcp->connect_timeout_ms = connect_timeout_ms;
    if (send_timeout_ms > 0) tcp->send_timeout_ms = send_timeout_ms;
}

#ifdef DOIP_MOCK_TP
int doip_tp_mock_init(DoIPTcpTransport *tcp, const char *ip, uint16_t port) {
    (void)tcp;
    (void)ip;
    (void)port;
    return 0;
}

int doip_tp_mock_connect(DoIPTcpTransport *tcp) {
    (void)tcp;
    return 0;
}

ssize_t doip_tp_mock_send(DoIPTcpTransport *tcp, const uint8_t *buf, size_t len) {
    (void)tcp;
    (void)buf;
    return (ssize_t)len;
}

ssize_t doip_tp_mock_recv(DoIPTcpTransport *tcp, uint8_t *buf, size_t len, int timeout_ms) {
    (void)tcp;
    (void)buf;
    (void)len;
    (void)timeout_ms;
    return 0;
}

void doip_tp_mock_close(DoIPTcpTransport *tcp) {
    (void)tcp;
}

#else

int doip_tp_tcp_init(DoIPTcpTransport *tcp, const char *ip, uint16_t port) {
    if (!tcp || !ip) return -1;

    tcp->fd = -1;
    tcp->port = port ? port : DOIP_TCP_PORT;
    snprintf(tcp->ip, sizeof(tcp->ip), "%s", ip);
    tcp->connect_timeout_ms = DOIP_DEFAULT_TIMEOUT_MS;
    tcp->send_timeout_ms = DOIP_DEFAULT_TIMEOUT_MS;
    return 0;
}

int doip_tp_tcp_connect(DoIPTcpTransport *tcp) {
    if (!tcp) return -1;
    tcp->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp->fd < 0) {
        return -1;
    }

    /* set non-blocking before connect to avoid blocking connect */
    int flags = fcntl(tcp->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(tcp->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(tcp->fd);
        tcp->fd = -1;
        return -1;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(tcp->port);
    if (inet_pton(AF_INET, tcp->ip, &sa.sin_addr) <= 0) {
        close(tcp->fd);
        tcp->fd = -1;
        return -1;
    }
    int rc = connect(tcp->fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0) {
        if (errno != EINPROGRESS) {
            close(tcp->fd);
            tcp->fd = -1;
            return -1;
        }
        /* wait for writability or error within default timeout */
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(tcp->fd, &wfds);
        struct timeval tv;
        int cto = (tcp->connect_timeout_ms > 0) ? tcp->connect_timeout_ms : DOIP_DEFAULT_TIMEOUT_MS;
        tv.tv_sec = cto / 1000;
        tv.tv_usec = (cto % 1000) * 1000;
        rc = select(tcp->fd + 1, NULL, &wfds, NULL, &tv);
        if (rc <= 0) {
            /* timeout or select error */
            close(tcp->fd);
            tcp->fd = -1;
            return -1;
        }
        int soerr = 0;
        socklen_t slen = sizeof(soerr);
        if (getsockopt(tcp->fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 || soerr != 0) {
            close(tcp->fd);
            tcp->fd = -1;
            return -1;
        }
    }
    return 0;
}

ssize_t doip_tp_tcp_send(const DoIPTcpTransport *tcp, const uint8_t *buf, size_t len) {
    if (!tcp || tcp->fd < 0 || !buf) return -1;
    size_t total = 0;
    int sflags = 0;
#ifdef MSG_NOSIGNAL
    sflags |= MSG_NOSIGNAL;
#endif
    while (total < len) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(tcp->fd, &wfds);
        struct timeval tv;
        int sto = (tcp->send_timeout_ms > 0) ? tcp->send_timeout_ms : DOIP_DEFAULT_TIMEOUT_MS;
        tv.tv_sec = sto / 1000;
        tv.tv_usec = (sto % 1000) * 1000;
        int rc = select(tcp->fd + 1, NULL, &wfds, NULL, &tv);
        if (rc <= 0) {
            /* timeout or error */
            return -1;
        }
        ssize_t n = send(tcp->fd, buf + total, len - total, sflags);
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

ssize_t doip_tp_tcp_recv(DoIPTcpTransport *tcp, uint8_t *buf, size_t len, int timeout_ms) {
    if (!tcp || tcp->fd < 0 || !buf) return -1;

    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(tcp->fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(tcp->fd + 1, &rfds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
    if (ret < 0) {
        return -1;
    }
    if (ret == 0) {
        return 0; /* timeout */
    }
    return recv(tcp->fd, buf, len, 0);
}

void doip_tp_tcp_close(DoIPTcpTransport *tcp) {
    if (!tcp) return;
    if (tcp->fd >= 0) {
        close(tcp->fd);
        tcp->fd = -1;
    }
}
#endif /* DOIP_MOCK_TP */

#endif /* UDS_TP_DOIP */
