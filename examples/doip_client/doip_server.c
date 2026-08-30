/**
 * @file doip_server.c
 * @brief ISO 13400 (DoIP) Transport Layer - Server Implementation
 * @details DoIP TCP transport layer server for UDS (ISO 14229)
 *
 * This implementation provides a DoIP transport layer for the iso14229 library.
 * It implements the minimal DoIP protocol features required for diagnostic communication:
 * - TCP-based communication on port 13400
 * - Routing activation
 * - Diagnostic message handling
 * - Alive check
 *
 * @note This is a simplified implementation focusing on TCP diagnostic messages.
 *       UDP vehicle discovery and other advanced features are not included.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>

/* DoIP Protocol Constants */
#define DOIP_PROTOCOL_VERSION           0x03
#define DOIP_PROTOCOL_VERSION_INV       0xFC
#define DOIP_TCP_PORT                   13400
#define DOIP_HEADER_SIZE                8

/* DoIP Payload Types */
#define DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ    0x0005
#define DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RES    0x0006
#define DOIP_PAYLOAD_TYPE_ALIVE_CHECK_REQ           0x0007
#define DOIP_PAYLOAD_TYPE_ALIVE_CHECK_RES           0x0008
#define DOIP_PAYLOAD_TYPE_DIAG_MESSAGE              0x8001
#define DOIP_PAYLOAD_TYPE_DIAG_MESSAGE_POS_ACK      0x8002
#define DOIP_PAYLOAD_TYPE_DIAG_MESSAGE_NEG_ACK      0x8003

/* DoIP Routing Activation Response Codes */
#define DOIP_ROUTING_ACTIVATION_RES_SUCCESS         0x10
#define DOIP_ROUTING_ACTIVATION_RES_UNKNOWN_SA      0x00
#define DOIP_ROUTING_ACTIVATION_RES_ALREADY_ACTIVE  0x01

/* DoIP Diagnostic Message NACK Codes */
#define DOIP_DIAG_NACK_INVALID_SA                   0x02
#define DOIP_DIAG_NACK_UNKNOWN_TA                   0x03
#define DOIP_DIAG_NACK_MESSAGE_TOO_LARGE            0x04
#define DOIP_DIAG_NACK_OUT_OF_MEMORY                0x05
#define DOIP_DIAG_NACK_TARGET_UNREACHABLE           0x06

/* Configuration */
#define DOIP_MAX_CLIENTS                8
#define DOIP_BUFFER_SIZE                4096
#define DOIP_LOGICAL_ADDRESS_SERVER     0x0001
#define DOIP_ROUTING_ACTIVATION_TYPE    0x00

/* DoIP Header Structure */
typedef struct {
    uint8_t protocol_version;
    uint8_t protocol_version_inv;
    uint16_t payload_type;
    uint32_t payload_length;
} __attribute__((packed)) DoIPHeader_t;

/* Client Connection State */
typedef struct {
    int socket_fd;
    bool active;
    bool routing_activated;
    uint16_t source_address;
    uint8_t rx_buffer[DOIP_BUFFER_SIZE];
    size_t rx_offset;
    uint8_t tx_buffer[DOIP_BUFFER_SIZE];
    size_t tx_offset;
} DoIPClientConnection_t;

/* DoIP Server Context */
typedef struct {
    int listen_socket;
    uint16_t logical_address;
    DoIPClientConnection_t clients[DOIP_MAX_CLIENTS];

    /* UDS integration callbacks */
    void (*on_diag_message)(uint16_t source_addr, const uint8_t *data, size_t len);
    void *user_data;
} DoIPServer_t;

/* Static server instance */
static DoIPServer_t server = {0};

/**
 * @brief Create and initialize DoIP header
 */
static void doip_header_init(DoIPHeader_t *header, uint16_t payload_type, uint32_t payload_length) {
    header->protocol_version = DOIP_PROTOCOL_VERSION;
    header->protocol_version_inv = DOIP_PROTOCOL_VERSION_INV;
    header->payload_type = htons(payload_type);
    header->payload_length = htonl(payload_length);
}

/**
 * @brief Parse DoIP header from buffer
 */
static bool doip_header_parse(const uint8_t *buffer, DoIPHeader_t *header) {
    if (!buffer || !header) {
        return false;
    }

    memcpy(header, buffer, sizeof(DoIPHeader_t));
    header->payload_type = ntohs(header->payload_type);
    header->payload_length = ntohl(header->payload_length);

    /* Validate protocol version */
    if (header->protocol_version != DOIP_PROTOCOL_VERSION ||
        header->protocol_version_inv != DOIP_PROTOCOL_VERSION_INV) {
        return false;
    }

    return true;
}

/**
 * @brief Send DoIP message to client
 */
static int doip_send_message(DoIPClientConnection_t *client, uint16_t payload_type,
                             const uint8_t *payload, uint32_t payload_len) {
    uint8_t buffer[DOIP_BUFFER_SIZE];
    DoIPHeader_t *header = (DoIPHeader_t *)buffer;

    if (DOIP_HEADER_SIZE + payload_len > DOIP_BUFFER_SIZE) {
        return -1;
    }

    doip_header_init(header, payload_type, payload_len);

    if (payload && payload_len > 0) {
        memcpy(buffer + DOIP_HEADER_SIZE, payload, payload_len);
    }

    ssize_t sent = send(client->socket_fd, buffer, DOIP_HEADER_SIZE + payload_len, 0);
    if (sent < 0) {
        perror("send");
        return -1;
    }

    return 0;
}

/**
 * @brief Handle routing activation request
 */
static void doip_handle_routing_activation(DoIPClientConnection_t *client,
                                           const uint8_t *payload, uint32_t payload_len) {
    if (payload_len < 7) {
        return;
    }

    uint16_t source_address = (payload[0] << 8) | payload[1];
    uint8_t activation_type = payload[2];
    (void)activation_type;  /* Currently unused, reserved for future use */

    /* Build routing activation response */
    uint8_t response[13];
    response[0] = (source_address >> 8) & 0xFF;      /* Client source address */
    response[1] = source_address & 0xFF;
    response[2] = (server.logical_address >> 8) & 0xFF;  /* Server logical address */
    response[3] = server.logical_address & 0xFF;
    response[4] = DOIP_ROUTING_ACTIVATION_RES_SUCCESS;     /* Response code */
    response[5] = 0x00;  /* Reserved */
    response[6] = 0x00;
    response[7] = 0x00;
    response[8] = 0x00;

    /* Optional: OEM specific data */
    response[9] = 0x00;
    response[10] = 0x00;
    response[11] = 0x00;
    response[12] = 0x00;

    client->routing_activated = true;
    client->source_address = source_address;

    doip_send_message(client, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RES, response, 13);

    printf("DoIP-server: Routing activated for SA=0x%04X\n", source_address);
}

/**
 * @brief Handle alive check request
 */
static void doip_handle_alive_check(DoIPClientConnection_t *client) {
    uint8_t response[2];
    response[0] = (server.logical_address >> 8) & 0xFF;
    response[1] = server.logical_address & 0xFF;

    doip_send_message(client, DOIP_PAYLOAD_TYPE_ALIVE_CHECK_RES, response, 2);
}

/**
 * @brief Handle diagnostic message
 */
static void doip_handle_diag_message(DoIPClientConnection_t *client,
                                     const uint8_t *payload, uint32_t payload_len) {
    if (!client->routing_activated) {
        /* Send NACK - routing not activated */
        uint8_t nack[5] = {0};
        nack[0] = (client->source_address >> 8) & 0xFF;
        nack[1] = client->source_address & 0xFF;
        nack[2] = (server.logical_address >> 8) & 0xFF;
        nack[3] = server.logical_address & 0xFF;
        nack[4] = DOIP_DIAG_NACK_TARGET_UNREACHABLE;
        doip_send_message(client, DOIP_PAYLOAD_TYPE_DIAG_MESSAGE_NEG_ACK, nack, 5);
        return;
    }

    if (payload_len < 4) {
        return;
    }

    uint16_t source_address = (payload[0] << 8) | payload[1];
    uint16_t target_address = (payload[2] << 8) | payload[3];

    /* Verify target address matches our logical address */
    if (target_address != server.logical_address) {
        uint8_t nack[5];
        nack[0] = (source_address >> 8) & 0xFF;
        nack[1] = source_address & 0xFF;
        nack[2] = (target_address >> 8) & 0xFF;
        nack[3] = target_address & 0xFF;
        nack[4] = DOIP_DIAG_NACK_UNKNOWN_TA;
        doip_send_message(client, DOIP_PAYLOAD_TYPE_DIAG_MESSAGE_NEG_ACK, nack, 5);
        printf("DoIP-server: Diagnostic message with unknown TA=0x%04X, expected 0x%04X\n", target_address, server.logical_address);
        return;
    }

    /* Send positive ACK */
    uint8_t ack[5];
    ack[0] = (source_address >> 8) & 0xFF;
    ack[1] = source_address & 0xFF;
    ack[2] = (target_address >> 8) & 0xFF;
    ack[3] = target_address & 0xFF;
    ack[4] = 0x00;  /* ACK code */
    doip_send_message(client, DOIP_PAYLOAD_TYPE_DIAG_MESSAGE_POS_ACK, ack, 5);

    /* Pass UDS data to application callback */
    if (server.on_diag_message && payload_len > 4) {
        const uint8_t *uds_data = payload + 4;
        size_t uds_len = payload_len - 4;
        server.on_diag_message(source_address, uds_data, uds_len);
    }
}

/**
 * @brief Process received DoIP message
 */
static void doip_process_message(DoIPClientConnection_t *client,
                                const DoIPHeader_t *header,
                                const uint8_t *payload) {
    printf("DoIP-server: Received payload type 0x%04X\n", header->payload_type);
    switch (header->payload_type) {
        case DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ:
            doip_handle_routing_activation(client, payload, header->payload_length);
            break;

        case DOIP_PAYLOAD_TYPE_ALIVE_CHECK_REQ:
            doip_handle_alive_check(client);
            break;

        case DOIP_PAYLOAD_TYPE_DIAG_MESSAGE:
            doip_handle_diag_message(client, payload, header->payload_length);
            break;

        default:
            printf("DoIP-server: Unknown payload type 0x%04X\n", header->payload_type);
            break;
    }
}

/**
 * @brief Handle client data reception
 */
static void doip_handle_client_rx(DoIPClientConnection_t *client) {
    ssize_t bytes_read = recv(client->socket_fd,
                              client->rx_buffer + client->rx_offset,
                              DOIP_BUFFER_SIZE - client->rx_offset, 0);

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("DoIP-server: Client disconnected\n");
        } else {
            perror("recv");
        }
        close(client->socket_fd);
        client->active = false;
        client->routing_activated = false;
        return;
    }

    client->rx_offset += bytes_read;

    /* Process complete DoIP messages */
    while (client->rx_offset >= DOIP_HEADER_SIZE) {
        DoIPHeader_t header;
        if (!doip_header_parse(client->rx_buffer, &header)) {
            printf("DoIP-server: Invalid header\n");
            close(client->socket_fd);
            client->active = false;
            return;
        }

        size_t total_msg_size = DOIP_HEADER_SIZE + header.payload_length;

        if (client->rx_offset < total_msg_size) {
            /* Wait for more data */
            break;
        }

        /* Process message */
        const uint8_t *payload = client->rx_buffer + DOIP_HEADER_SIZE;
        doip_process_message(client, &header, payload);

        /* Remove processed message from buffer */
        if (client->rx_offset > total_msg_size) {
            memmove(client->rx_buffer,
                   client->rx_buffer + total_msg_size,
                   client->rx_offset - total_msg_size);
        }
        client->rx_offset -= total_msg_size;
    }
}

/**
 * @brief Initialize DoIP server
 */
int doip_server_init(uint16_t logical_address,
                     void (*diag_msg_callback)(uint16_t, const uint8_t*, size_t)) {
    memset(&server, 0, sizeof(DoIPServer_t));

    server.logical_address = logical_address;
    server.on_diag_message = diag_msg_callback;

    /* Create TCP socket */
    server.listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server.listen_socket < 0) {
        perror("socket");
        return -1;
    }

    /* Set socket options */
    int opt = 1;
    if (setsockopt(server.listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server.listen_socket);
        return -1;
    }

    /* Bind to DoIP port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DOIP_TCP_PORT);

    if (bind(server.listen_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server.listen_socket);
        return -1;
    }

    /* Listen for connections */
    if (listen(server.listen_socket, DOIP_MAX_CLIENTS) < 0) {
        perror("listen");
        close(server.listen_socket);
        return -1;
    }

    printf("DoIP Server: Listening on port %d (LA=0x%04X)\n",
           DOIP_TCP_PORT, logical_address);

    return 0;
}

/**
 * @brief Accept new client connection
 */
static void doip_accept_connection(void) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(server.listen_socket,
                          (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        return;
    }

    /* Find free client slot */
    DoIPClientConnection_t *client = NULL;
    for (int i = 0; i < DOIP_MAX_CLIENTS; i++) {
        if (!server.clients[i].active) {
            client = &server.clients[i];
            break;
        }
    }

    if (!client) {
        printf("DoIP-server: Max clients reached, rejecting connection\n");
        close(client_fd);
        return;
    }

    /* Initialize client */
    memset(client, 0, sizeof(DoIPClientConnection_t));
    client->socket_fd = client_fd;
    client->active = true;
    client->routing_activated = false;

    printf("DoIP-server: Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}

/**
 * @brief Send diagnostic message response to client
 */
int doip_server_send_diag_response(uint16_t source_address,
                                   const uint8_t *data, size_t len) {
    /* Find client with matching source address */
    DoIPClientConnection_t *client = NULL;
    for (int i = 0; i < DOIP_MAX_CLIENTS; i++) {
        if (server.clients[i].active &&
            server.clients[i].routing_activated &&
            server.clients[i].source_address == source_address) {
            client = &server.clients[i];
            break;
        }
    }

    if (!client) {
        printf("DoIP-server: No active client with SA=0x%04X\n", source_address);
        return -1;
    }

    /* Build diagnostic message payload */
    uint8_t payload[DOIP_BUFFER_SIZE];
    if (len + 4 > DOIP_BUFFER_SIZE) {
        return -1;
    }

    payload[0] = (server.logical_address >> 8) & 0xFF;  /* Source address (server) */
    payload[1] = server.logical_address & 0xFF;
    payload[2] = (source_address >> 8) & 0xFF;            /* Target address (client) */
    payload[3] = source_address & 0xFF;
    memcpy(payload + 4, data, len);

    return doip_send_message(client, DOIP_PAYLOAD_TYPE_DIAG_MESSAGE, payload, len + 4);
}

/**
 * @brief Process DoIP server events (call periodically)
 */
void doip_server_process(int timeout_ms) {
    fd_set readfds;
    struct timeval tv;
    int max_fd = server.listen_socket;

    FD_ZERO(&readfds);
    FD_SET(server.listen_socket, &readfds);

    /* Add active client sockets */
    for (int i = 0; i < DOIP_MAX_CLIENTS; i++) {
        if (server.clients[i].active) {
            FD_SET(server.clients[i].socket_fd, &readfds);
            if (server.clients[i].socket_fd > max_fd) {
                max_fd = server.clients[i].socket_fd;
            }
        }
    }

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0) {
        perror("select");
        return;
    }

    if (ret == 0) {
        return;  /* Timeout */
    }

    /* Check for new connections */
    if (FD_ISSET(server.listen_socket, &readfds)) {
        doip_accept_connection();
    }

    /* Check client sockets */
    for (int i = 0; i < DOIP_MAX_CLIENTS; i++) {
        if (server.clients[i].active &&
            FD_ISSET(server.clients[i].socket_fd, &readfds)) {
            doip_handle_client_rx(&server.clients[i]);
        }
    }
}

/**
 * @brief Shutdown DoIP server
 */
void doip_server_shutdown(void) {
    /* Close all client connections */
    for (int i = 0; i < DOIP_MAX_CLIENTS; i++) {
        if (server.clients[i].active) {
            close(server.clients[i].socket_fd);
        }
    }

    /* Close listen socket */
    if (server.listen_socket >= 0) {
        close(server.listen_socket);
    }

    printf("DoIP Server: Shutdown complete\n");
}
