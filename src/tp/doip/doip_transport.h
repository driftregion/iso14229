#ifndef DOIP_TRANSPORT_H
#define DOIP_TRANSPORT_H
#if defined(UDS_TP_DOIP)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declaration */
typedef struct DoIPTcpTransport DoIPTcpTransport;
typedef struct DoIPUdpTransport DoIPUdpTransport;

#ifdef DOIP_MOCK_TP
/* Mock transport functions for testing. For now these method do nothing and return
 * success.
 */

int doip_tp_mock_init(DoIPTcpTransport *tcp, const char *ip, uint16_t port);
int doip_tp_mock_connect(DoIPTcpTransport *tcp);
ssize_t doip_tp_mock_send(DoIPTcpTransport *tcp, const uint8_t *buf, size_t len);
ssize_t doip_tp_mock_recv(DoIPTcpTransport *tcp, uint8_t *buf, size_t len, int timeout_ms);
void doip_tp_mock_close(DoIPTcpTransport *tcp);

int doip_tp_mock_udp_init(DoIPUdpTransport *t, uint16_t port, bool loopback);
ssize_t doip_tp_mock_udp_recv(DoIPUdpTransport *t, uint8_t *buf, size_t len, int timeout_ms);
ssize_t doip_tp_mock_udp_recvfrom(DoIPUdpTransport *t, uint8_t *buf, size_t len, int timeout_ms,
                                  char *src_ip_out, size_t src_ip_out_sz, uint16_t *src_port_out);
void doip_tp_mock_udp_close(DoIPUdpTransport *t);
ssize_t doip_tp_mock_udp_sendto(DoIPUdpTransport *t, const uint8_t *buf, size_t len,
                                const char *dst_ip, uint16_t dst_port, int timeout_ms);

int doip_tp_mock_udp_join_default_multicast(DoIPUdpTransport *t);
#else

/**
 * @brief Initialize DoIP TCP transport
 *
 * @param t Transport context
 * @param ip Remote IP address
 * @param port Remote port
 * @retval 0 success
 * @retval -1 error
 */
int doip_tp_tcp_init(DoIPTcpTransport *tcp, const char *ip, uint16_t port);

/**
 * @brief Connect DoIP TCP transport
 *
 * @param t Transport context
 * @retval 0 success
 * @retval -1 error
 */
int doip_tp_tcp_connect(DoIPTcpTransport *tcp);

/**
 * @brief Send data over DoIP TCP transport
 *
 * @param t Transport context
 * @param buf Data buffer to send
 * @param len Length of data to send
 * @return ssize_t Number of bytes sent, or -1 on error
 */
ssize_t doip_tp_tcp_send(const DoIPTcpTransport *tcp, const uint8_t *buf, size_t len);

/**
 * @brief Receive data over DoIP TCP transport
 *
 * @param t Transport context
 * @param buf Buffer to receive data into
 * @param len Length of buffer
 * @param timeout_ms Receive timeout in milliseconds
 * @return ssize_t Number of bytes received, 0 on timeout, or -1 on error
 */
ssize_t doip_tp_tcp_recv(DoIPTcpTransport *tcp, uint8_t *buf, size_t len, int timeout_ms);

/**
 * @brief Close DoIP TCP transport
 *
 * @param t Transport context
 */
void doip_tp_tcp_close(DoIPTcpTransport *tcp);

/* Optional: configure timeouts on the transport */
/**
 * @brief Set timeouts for DoIP transport
 * @param t Transport context
 * @param connect_timeout_ms Connect timeout in milliseconds (<=0 for default)
 * @param send_timeout_ms Send timeout in milliseconds (<=0 for default)
 */
void doip_tp_set_timeouts(DoIPTcpTransport *tcp, int connect_timeout_ms, int send_timeout_ms);

/* UDP transport helpers (vehicle discovery) */

/**
 * @brief Initialize DoIP UDP transport
 *
 * @param t Transport context
 * @param port Local port
 * @param loopback Enable loopback mode (instead of multicast)
 * @retval 0 success
 * @retval -1 error
 */
int doip_tp_udp_init(DoIPUdpTransport *t, uint16_t port, bool loopback);

/**
 * @brief Join default DoIP multicast group for discovery
 *
 * @param t Transport context
 * @retval 0 success
 * @retval -1 error
 */
int doip_tp_udp_join_default_multicast(DoIPUdpTransport *t);

/**
 * @brief Receive data over DoIP UDP transport
 *
 * @param t Transport context
 * @param buf Buffer to receive data into
 * @param len Length of buffer
 * @param timeout_ms Receive timeout in milliseconds
 * @return ssize_t Number of bytes received, 0 on timeout, or -1 on error
 */
ssize_t doip_tp_udp_recv(DoIPUdpTransport *t, uint8_t *buf, size_t len, int timeout_ms);

/**
 * @brief Receive data over DoIP UDP transport with source address info
 *
 * @param t Transport context
 * @param buf Buffer to receive data into
 * @param len Length of buffer
 * @param timeout_ms Receive timeout in milliseconds
 * @param src_ip_out Output buffer for source IP address
 * @param src_ip_out_sz Size of source IP output buffer
 * @param src_port_out Output for source port
 * @return ssize_t Number of bytes received, 0 on timeout, or -1 on error
 */
ssize_t doip_tp_udp_recvfrom(DoIPUdpTransport *t, uint8_t *buf, size_t len, int timeout_ms,
                             char *src_ip_out, size_t src_ip_out_sz, uint16_t *src_port_out);

/**
 * @brief Close DoIP UDP transport
 *
 * @param t Transport context
 */
void doip_tp_udp_close(DoIPUdpTransport *t);

/* UDP send helper */
/**
 * @brief Send data over DoIP UDP transport
 *
 * @param t Transport context
 * @param buf Data buffer to send
 * @param len Length of data to send
 * @param dst_ip Destination IP address
 * @param dst_port Destination port
 * @param timeout_ms Send timeout in milliseconds
 * @return ssize_t Number of bytes sent, or -1 on error
 */
ssize_t doip_tp_udp_sendto(DoIPUdpTransport *t, const uint8_t *buf, size_t len, const char *dst_ip,
                           uint16_t dst_port, int timeout_ms);

#endif /* DOIP_MOCK_TP */

/* TCP transport function pointers */
typedef int (*tcp_init)(DoIPTcpTransport *tcp, const char *ip, uint16_t port);
typedef int (*tcp_connect)(DoIPTcpTransport *tcp);
typedef ssize_t (*tcp_send)(const DoIPTcpTransport *tcp, const uint8_t *buf, size_t len);
typedef ssize_t (*tcp_recv)(DoIPTcpTransport *tcp, uint8_t *buf, size_t len, int timeout_ms);
typedef void (*tcp_close)(DoIPTcpTransport *tcp);

/* UDP transport function pointers */
typedef int (*udp_init)(DoIPUdpTransport *t, uint16_t port, bool loopback);
typedef ssize_t (*udp_recv)(DoIPUdpTransport *t, uint8_t *buf, size_t len, int timeout_ms);
typedef ssize_t (*udp_recvfrom)(DoIPUdpTransport *t, uint8_t *buf, size_t len, int timeout_ms,
                                char *src_ip_out, size_t src_ip_out_sz, uint16_t *src_port_out);
typedef ssize_t (*udp_sendto)(DoIPUdpTransport *t, const uint8_t *buf, size_t len, const char *dst_ip,
                              uint16_t dst_port, int timeout_ms);
typedef void (*udp_close)(DoIPUdpTransport *t);
typedef int (*udp_join_multicast)(DoIPUdpTransport *t);


typedef struct DoIPTcpTransport {
    int fd;
    int connect_timeout_ms; /* connect timeout (ms), <=0 uses default */
    int send_timeout_ms;    /* send timeout (ms), <=0 uses default */
    char ip[64];   /* remote IP (for TCP) */
    uint16_t port; /* local or remote port */
    tcp_init init;
    tcp_connect connect;
    tcp_send send;
    tcp_recv recv;
    tcp_close close;
} DoIPTcpTransport;

typedef struct DoIPUdpTransport {
    int fd;
    uint16_t port; /* local or remote port */
    udp_init init;
    udp_recv recv;
    udp_recvfrom recvfrom;
    udp_sendto sendto;
    udp_close close;
    udp_join_multicast join_multicast;
    bool loopback; /* UDP loopback mode */
} DoIPUdpTransport;

#endif /* UDS_TP_DOIP */

#endif /* DOIP_TRANSPORT_H */
