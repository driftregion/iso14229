#ifndef DOIP_TRANSPORT_H
#define DOIP_TRANSPORT_H
#if defined(UDS_TP_DOIP)


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Minimal transport abstraction for DoIP */
typedef struct DoIPTransport {
    int fd;                /* OS socket descriptor */
    uint16_t port;         /* local or remote port */
    char ip[64];           /* remote IP (for TCP) */
    bool is_udp;           /* transport type flag */
    bool loopback;         /* UDP loopback mode */
    int connect_timeout_ms;/* connect timeout (ms), <=0 uses default */
    int send_timeout_ms;   /* send timeout (ms), <=0 uses default */
} DoIPTransport;

WIP: Assemble struct for tcp and udp function pointers
typedef (*tcp_init)(DoIPTransport *t, const char *ip, uint16_t port);

typedef struct DoIPTcpTransport {
    tcp_init init;
} DoIPTcpTransport;

/* TCP transport helpers */
int doip_tp_tcp_init(DoIPTransport *t, const char *ip, uint16_t port);
int doip_tp_tcp_connect(DoIPTransport *t);
ssize_t doip_tp_tcp_send(DoIPTransport *t, const uint8_t *buf, size_t len);
ssize_t doip_tp_tcp_recv(DoIPTransport *t, uint8_t *buf, size_t len, int timeout_ms);
void doip_tp_tcp_close(DoIPTransport *t);

/* Optional: configure timeouts on the transport */
void doip_tp_set_timeouts(DoIPTransport *t, int connect_timeout_ms, int send_timeout_ms);

/* UDP transport helpers (vehicle discovery) */
int doip_tp_udp_init(DoIPTransport *t, uint16_t port, bool loopback);
int doip_tp_udp_join_default_multicast(DoIPTransport *t);
ssize_t doip_tp_udp_recv(DoIPTransport *t, uint8_t *buf, size_t len, int timeout_ms);
/* Receive with source address info */
ssize_t doip_tp_udp_recvfrom(DoIPTransport *t, uint8_t *buf, size_t len, int timeout_ms,
                             char *src_ip_out, size_t src_ip_out_sz, uint16_t *src_port_out);
void doip_tp_udp_close(DoIPTransport *t);

/* UDP send helper */
ssize_t doip_tp_udp_sendto(DoIPTransport *t, const uint8_t *buf, size_t len,
                           const char *dst_ip, uint16_t dst_port, int timeout_ms);

#endif /* UDS_TP_DOIP */


#endif /* DOIP_TRANSPORT_H */
