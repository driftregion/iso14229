#if defined(UDS_TP_DOIP)
/**
 * @file doip_client.c
 * @brief ISO 13400 (DoIP) Transport Layer - Client Implementation
 * @details DoIP TCP transport layer client for UDS (ISO 14229)
 *
 *
 * @note This is a simplified implementation focusing on TCP diagnostic messages.
 *       UDP vehicle discovery is not included.
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
#include <time.h>

#include "doip_client.h"
#include "util.h"
#include "doip_transport.h"
#include "log.h"

/* Macro to extract 16-bit BE DoIP address from buffer */
#define DOIP_ADDRESS(a, off) ((uint16_t)(((a[(off)]) << 8) | (a[(off) + 1])))

/**
 * @brief Initialize DoIP header
 * @param header Pointer to DoIPHeader_t structure to initialize
 * @param payload_type DoIP payload type
 * @param payload_length Length of DoIP payload
 */
static void doip_header_init(DoIPHeader_t *header, uint16_t payload_type, uint32_t payload_length) {
    header->protocol_version = DOIP_PROTOCOL_VERSION;
    header->protocol_version_inv = DOIP_PROTOCOL_VERSION_INV;
    header->payload_type = htons(payload_type);
    header->payload_length = htonl(payload_length);
}

/**
 * @brief Convert DoIP client state to string.
 *
 * @param state the state to convert
 * @return const char* the string representation of the state
 */
const char *doip_client_state_to_string(DoIPClientState_t state) {
    switch (state) {
    case DOIP_STATE_DISCONNECTED:
        return "DISCONNECTED";
    case DOIP_STATE_CONNECTED:
        return "CONNECTED";
    case DOIP_STATE_ROUTING_ACTIVATION_PENDING:
        return "ROUTING_ACTIVATION_PENDING";
    case DOIP_STATE_READY_FOR_DIAG_REQUEST:
        return "READY";
    case DOIP_STATE_DIAG_MESSAGE_ACK_PENDING:
        return "ACK_PENDING";
    case DOIP_STATE_DIAG_MESSAGE_RESPONSE_PENDING:
        return "RESPONSE_PENDING";
    case DOIP_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN_STATE";
    }
}

/**
 * @brief Helper macro to change DoIP client state with logging.
 */
#define doip_change_state(t, s)                                                                    \
    {                                                                                              \
        if (_doip_change_state((t), (s))) {                                                        \
            UDS_LOGV(__FILE__, "DoIP: State change to %s (line %d)",                               \
                     doip_client_state_to_string(s), __LINE__);                                    \
        }                                                                                          \
    }

/**
 * @brief Change DoIP client state
 * @param tp DoIP client context
 * @param new_state New state to set
 * @return true if state changed, false if same state
 */
static bool _doip_change_state(DoIPClient_t *tp, DoIPClientState_t new_state) {
    if (tp->state == new_state)
        return false;

    // UDS_LOGI(__FILE__, "DoIP: State change %d -> %d", tp->state, new_state);
    tp->state = new_state;
    return true;
}

/**
 * @brief Parse DoIP header from buffer
 * @param buffer Pointer to buffer containing DoIP header
 * @param header Pointer to DoIPHeader_t structure to fill
 */
static bool doip_header_parse(const uint8_t *buffer, DoIPHeader_t *header) {
    if (NULL == buffer || NULL == header) {
        return false;
    }

    header->protocol_version = buffer[0];
    header->protocol_version_inv = buffer[1];
    header->payload_type = buffer[2] << 8 | buffer[3];
    header->payload_length = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7];

    /* Validate protocol version */
    if (header->protocol_version != DOIP_PROTOCOL_VERSION ||
        header->protocol_version_inv != DOIP_PROTOCOL_VERSION_INV) {
        return false;
    }

    return true;
}

/**
 * @brief Send DoIP message.
 * @param tp DoIP client context
 * @param payload_type DoIP payload type
 * @param payload Pointer to DoIP payload
 * @param payload_len Length of DoIP payload
 * @return int Number of payload bytes sent, or -1 on error
 */
static int doip_send_message(const DoIPClient_t *tp, uint16_t payload_type, const uint8_t *payload,
                             uint32_t payload_len) {
    uint8_t buffer[DOIP_BUFFER_SIZE];
    DoIPHeader_t *header = (DoIPHeader_t *)buffer;

    if (DOIP_HEADER_SIZE + payload_len > DOIP_BUFFER_SIZE) {
        return -1;
    }

    doip_header_init(header, payload_type, payload_len);

    if (payload && payload_len > 0) {
        memcpy(buffer + DOIP_HEADER_SIZE, payload, payload_len);
    }

    ssize_t sent = tp->tcp.send(&tp->tcp, buffer, DOIP_HEADER_SIZE + payload_len);
    if (sent < 0) {
        perror("tcp_send");
        return -1;
    }

    /* Return number of UDS payload bytes sent (strip headers) */
    return sent - DOIP_HEADER_SIZE - DOIP_DIAG_HEADER_SIZE;
}

/**
 * @brief Stores UDS response data in DoIP client context
 *
 * @param tp DoIP client context
 * @param data Pointer to DoIP response data. Assumes a diag message payload.
 * @param len Length of DoIP response data
 */
void doip_store_uds_response(DoIPClient_t *tp, const uint8_t *data, size_t len) {
    if (len > DOIP_BUFFER_SIZE) {
        UDS_LOGE(__FILE__, "DoIP: Response too large to store (%zu bytes)", len);
        return;
    }

    // Store UDS response data in separate buffer for doip_tp_recv to retrieve
    if (len > DOIP_DIAG_HEADER_SIZE) {
        uint16_t sa = DOIP_ADDRESS(data, 0);

        // strip diag header (sa and ta)
        const uint8_t *uds_data = data + DOIP_DIAG_HEADER_SIZE;
        size_t uds_len = len - DOIP_DIAG_HEADER_SIZE;

        /* Copy UDS data to uds_response buffer */
        if (uds_len <= DOIP_BUFFER_SIZE) {
            memcpy(tp->uds_response, uds_data, uds_len);
            tp->uds_response_len = uds_len;
            UDS_LOGI(__FILE__, "DoIP: Stored diagnostic response (%zu bytes) from SA=0x%04X",
                     uds_len, sa);
        } else {
            UDS_LOGE(__FILE__, "DoIP: Diagnostic response too large (%zu bytes)", uds_len);
        }
    }
}

/**
 * @brief Handle routing activation response
 * @param tp DoIP client context
 * @param payload Pointer to DoIP payload
 * @param payload_len Length of DoIP payload
 */
static void doip_handle_routing_activation_response(DoIPClient_t *tp, const uint8_t *payload,
                                                    uint32_t payload_len) {
    if (payload_len < 9) {
        UDS_LOGI(__FILE__, "DoIP: Invalid routing activation response length");
        doip_change_state(tp, DOIP_STATE_ERROR);
        return;
    }

    uint16_t client_sa = DOIP_ADDRESS(payload, 0);
    uint16_t server_sa = DOIP_ADDRESS(payload, 2);
    uint8_t response_code = payload[4];
    (void)client_sa; /* Validated implicitly by successful response */

    if (response_code == DOIP_ROUTING_ACTIVATION_RES_SUCCESS) {
        doip_change_state(tp, DOIP_STATE_READY_FOR_DIAG_REQUEST);
        UDS_LOGI(__FILE__, "DoIP: Routing activated (SA=0x%04X, TA=0x%04X)", tp->source_address,
                 server_sa);
    } else {
        UDS_LOGI(__FILE__, "DoIP: Routing activation failed (code=0x%02X)", response_code);
        doip_change_state(tp, DOIP_STATE_ERROR);
    }
}

/**
 * @brief Sends alive check response when (DoIP) server requests it.
 * @param tp DoIP client context
 * @param payload Pointer to DoIP payload
 * @param payload_len Length of DoIP payload
 */
static void doip_handle_alive_check_request(const DoIPClient_t *tp, const uint8_t *payload,
                                            uint32_t payload_len) {

    (void)tp;
    (void)payload;
    (void)payload_len;

    // alive check request payload is empty, the response contains the client's source address
    uint8_t response[2];
    response[0] = (tp->source_address >> 8) & 0xFF;
    response[1] = tp->source_address & 0xFF;

    UDS_LOGI(__FILE__, "DoIP: Alive check request -> response from 0x%04X", tp->source_address);
    int sent = doip_send_message(tp, DOIP_PAYLOAD_TYPE_ALIVE_CHECK_RES, response, sizeof(response));
    if (sent < 0) {
        UDS_LOGE(__FILE__, "DoIP: Failed to send alive check response");
    } else {
        UDS_LOGI(__FILE__, "DoIP: Sent alive check response (%d bytes)", sent);
    }
}

/**
 * @brief Handle diagnostic message positive ACK.
 * @param tp DoIP client context
 * @param payload Pointer to DoIP payload
 * @param payload_len Length of DoIP payload
 */
static void doip_handle_diag_pos_ack(DoIPClient_t *tp, const uint8_t *payload,
                                     uint32_t payload_len) {
    if (payload_len < 5) {
        return;
    }

    uint16_t source_address = DOIP_ADDRESS(payload, 0);
    uint16_t target_address = DOIP_ADDRESS(payload, 2);
    uint8_t ack_code = payload[4];

    tp->diag_ack_received = true;

    UDS_LOGI(__FILE__, "DoIP: Diagnostic message ACK (SA=0x%04X, TA=0x%04X, code=0x%02X)",
             source_address, target_address, ack_code);
}

/**
 * @brief Handle diagnostic message negative ACK.
 * @param tp DoIP client context
 * @param payload Pointer to DoIP payload
 * @param payload_len Length of DoIP payload
 */
static void doip_handle_diag_neg_ack(DoIPClient_t *tp, const uint8_t *payload,
                                     uint32_t payload_len) {
    if (payload_len < 5) {
        return;
    }

    uint16_t source_address = DOIP_ADDRESS(payload, 0);
    uint16_t target_address = DOIP_ADDRESS(payload, 2);
    uint8_t nack_code = payload[4];

    tp->diag_nack_received = true;
    tp->diag_nack_code = nack_code;

    UDS_LOGW(__FILE__, "DoIP: Diagnostic message NACK (SA=0x%04X, TA=0x%04X, code=0x%02X)",
             source_address, target_address, nack_code);
}

/**
 * @brief Handle diagnostic message (response from server).
 * @param tp DoIP client context
 * @param payload Pointer to DoIP payload
 * @param payload_len Length of DoIP payload
 */
static void doip_handle_diag_message(DoIPClient_t *tp, const uint8_t *payload,
                                     uint32_t payload_len) {
    if (payload_len < 4) {
        return;
    }

    uint16_t target_address = DOIP_ADDRESS(payload, 2);

    /* Verify target address matches our logical address */
    if (target_address != tp->source_address) {
        UDS_LOGI(__FILE__, "DoIP: Received diagnostic message for different TA=0x%04X",
                 target_address);
        return;
    }

    /* Store UDS response data in separate buffer */
    doip_store_uds_response(tp, payload, payload_len);
}

/**#ifdef DOIP_MOCK_TP
 * @brief Process received DoIP message according to payload type.
 * @param tp DoIP client context
 * @param header Pointer to DoIP header
 * @param payload Pointer to DoIP payload
 */
static void doip_process_message(DoIPClient_t *tp, const DoIPHeader_t *header,
                                 const uint8_t *payload) {
    switch (header->payload_type) {
    case DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RES:
        doip_handle_routing_activation_response(tp, payload, header->payload_length);
        break;

    case DOIP_PAYLOAD_TYPE_ALIVE_CHECK_REQ:
        doip_handle_alive_check_request(tp, payload, header->payload_length);
        break;

    case DOIP_PAYLOAD_TYPE_DIAG_MESSAGE_POS_ACK:
        doip_handle_diag_pos_ack(tp, payload, header->payload_length);
        break;

    case DOIP_PAYLOAD_TYPE_DIAG_MESSAGE_NEG_ACK:
        doip_handle_diag_neg_ack(tp, payload, header->payload_length);
        break;

    case DOIP_PAYLOAD_TYPE_DIAG_MESSAGE:
        doip_handle_diag_message(tp, payload, header->payload_length);
        break;

    default:
        UDS_LOGI(__FILE__, "DoIP: Unknown payload type 0x%04X", header->payload_type);
        break;
    }
}

/**
 * @brief Receive and process data
 * @param tp DoIP client context
 * @param timeout_ms Timeout in milliseconds
 */
static ssize_t doip_receive_data(DoIPClient_t *tp, int timeout_ms) {
    ssize_t bytes_read = tp->tcp.recv(&tp->tcp, tp->rx_buffer + tp->rx_offset,
                                      DOIP_BUFFER_SIZE - tp->rx_offset, timeout_ms);

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            UDS_LOGE(__FILE__, "DoIP: Server disconnected");
        } else {
            perror("tcp_recv");
        }
        doip_change_state(tp, DOIP_STATE_DISCONNECTED);
        return -1;
    }

    UDS_LOGI(__FILE__, "DoIP: Received %zd bytes", bytes_read);

    tp->rx_offset += bytes_read;
    /* Process complete DoIP messages */
    while (tp->rx_offset >= DOIP_HEADER_SIZE) {
        DoIPHeader_t header;
        if (!doip_header_parse(tp->rx_buffer, &header)) {
            UDS_LOGE(__FILE__, "DoIP: Invalid header");
            doip_change_state(tp, DOIP_STATE_ERROR);
            return -1;
        }

        size_t total_msg_size = DOIP_HEADER_SIZE + header.payload_length;

        if (tp->rx_offset < total_msg_size) {
            /* Wait for more data */
            break;
        }

        /* Process message */
        const uint8_t *payload = tp->rx_buffer + DOIP_HEADER_SIZE;
        UDS_LOGI(__FILE__, "DoIP: Processing message type 0x%04X, length %u", header.payload_type,
                 header.payload_length);
        UDS_LOG_SDU(__FILE__, payload, header.payload_length, NULL);
        doip_process_message(tp, &header, payload);

        /* Remove processed message from buffer */
        if (tp->rx_offset > total_msg_size) {
            memmove(tp->rx_buffer, tp->rx_buffer + total_msg_size, tp->rx_offset - total_msg_size);
        }
        tp->rx_offset -= total_msg_size;
    }

    return bytes_read;
}

/**
 * @brief Connect to DoIP server
 * @param tp DoIP client context
 */
int doip_client_connect(DoIPClient_t *tp) {
    if (tp->state != DOIP_STATE_DISCONNECTED) {
        UDS_LOGE(__FILE__, "DoIP: Already connected or in error state");
        return -1;
    }

    if (tp->tcp.init(&tp->tcp, tp->server_ip, tp->server_port ? tp->server_port : DOIP_TCP_PORT) <
        0) {
        UDS_LOGE(__FILE__, "DoIP: TCP init failed");
        return -1;
    }
    if (tp->tcp.connect(&tp->tcp) < 0) {
        UDS_LOGE(__FILE__, "DoIP: TCP connect error (%s:%d)", tp->server_ip,
                 tp->server_port ? tp->server_port : DOIP_TCP_PORT);
        return -1;
    }

    doip_change_state(tp, DOIP_STATE_CONNECTED);
    UDS_LOGI(__FILE__, "DoIP Client: Connected to %s:%d", tp->server_ip,
             tp->server_port ? tp->server_port : DOIP_TCP_PORT);

    return 0;
}

/**
 * @brief Activate routing. Sends a "routing activation" request to the server.
 * @param tp DoIP client context
 */
int doip_client_activate_routing(DoIPClient_t *tp) {
    if (tp->state != DOIP_STATE_CONNECTED) {
        UDS_LOGE(__FILE__, "DoIP: Not connected");
        return -1;
    }

    /* Build routing activation request */
    uint8_t payload[11];
    payload[0] = (tp->source_address >> 8) & 0xFF;
    payload[1] = tp->source_address & 0xFF;
    payload[2] = DOIP_ROUTING_ACTIVATION_TYPE;
    payload[3] = 0x00; /* Reserved */
    payload[4] = 0x00;
    payload[5] = 0x00;
    payload[6] = 0x00;

    /* Optional: OEM specific */
    payload[7] = 0x00;
    payload[8] = 0x00;
    payload[9] = 0x00;
    payload[10] = 0x00;

    if (doip_send_message(tp, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ, payload, 11) < 0) {
        return -1;
    }

    doip_change_state(tp, DOIP_STATE_ROUTING_ACTIVATION_PENDING);

    /* Wait for routing activation response */
    int timeout_ms = DOIP_DEFAULT_TIMEOUT_MS;
    clock_t start = clock();

    while (tp->state == DOIP_STATE_ROUTING_ACTIVATION_PENDING) {
        int elapsed_ms = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
        int remaining_ms = timeout_ms - elapsed_ms;

        if (remaining_ms <= 0) {
            UDS_LOGE(__FILE__, "DoIP: Routing activation timeout");
            doip_change_state(tp, DOIP_STATE_ERROR);
            return -1;
        }

        if (doip_receive_data(tp, remaining_ms) < 0) {
            return -1;
        }
    }

    if (tp->state != DOIP_STATE_READY_FOR_DIAG_REQUEST) {
        UDS_LOGE(__FILE__, "DoIP: Routing activation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief Send diagnostic (UDS) message via DoIP. The UDS message is wrapped in a DoIP diagnostic
 * message, consisting of a DoIP header and a diagnostic message header (source and target
 * addresses).
 * @param tp DoIP client context
 * @param data Pointer to diagnostic message data. This is the UDS payload.
 * @param len Length of diagnostic message data (of UDS payload)
 */
ssize_t doip_client_send_diag_message(DoIPClient_t *tp, const uint8_t *data, size_t len) {
    /* Build diagnostic message payload */
    uint8_t payload[DOIP_BUFFER_SIZE];
    if (len + 4 > DOIP_BUFFER_SIZE) {
        UDS_LOGE(__FILE__, "DoIP: Message too large: %zu bytes > %d", len + 4, DOIP_BUFFER_SIZE);
        return -1;
    }

    doip_change_state(tp, DOIP_STATE_DIAG_MESSAGE_SEND_PENDING);

    /* Add diagnostic message header (source and target addresses) */
    payload[0] = (tp->source_address >> 8) & 0xFF;
    payload[1] = tp->source_address & 0xFF;
    payload[2] = (tp->target_address >> 8) & 0xFF;
    payload[3] = tp->target_address & 0xFF;
    memcpy(payload + 4, data, len);

    /* Reset ACK/NACK flags */
    tp->diag_ack_received = false;
    tp->diag_nack_received = false;

    int sent = doip_send_message(tp, DOIP_PAYLOAD_TYPE_DIAG_MESSAGE, payload, len + 4);

    if (sent < 0) {
        return -1;
    }

    /* Wait for ACK/NACK of DoIP server*/
    doip_change_state(tp, DOIP_STATE_DIAG_MESSAGE_ACK_PENDING);
    int timeout_ms = DOIP_ACK_TIMEOUT_MS; /* 1 second for ACK */
    clock_t start = clock();

    while (!tp->diag_ack_received && !tp->diag_nack_received) {
        int elapsed_ms = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
        int remaining_ms = timeout_ms - elapsed_ms;

        if (remaining_ms <= 0) {
            UDS_LOGE(__FILE__, "DoIP: Diagnostic message ACK timeout");
            return -1;
        }

        if (doip_receive_data(tp, remaining_ms) < 0) {
            return -1;
        }
    }

    // NACK received -> report error and fall back to idle state
    if (tp->diag_nack_received) {
        doip_change_state(tp, DOIP_STATE_READY_FOR_DIAG_REQUEST);
        UDS_LOGE(__FILE__, "DoIP: Diagnostic message rejected (NACK code=0x%02X)",
                 tp->diag_nack_code);
        return -1;
    }

    doip_change_state(tp, DOIP_STATE_DIAG_MESSAGE_RESPONSE_PENDING);

    return sent;
}

/**
 * @brief Process DoIP client events (call periodically).
 * @param tp DoIP client context
 * @param timeout_ms Timeout in milliseconds for receiving data
 */
void doip_client_process(DoIPClient_t *tp, int timeout_ms) {
    if (tp->state == DOIP_STATE_READY_FOR_DIAG_REQUEST) {
        doip_receive_data(tp, timeout_ms);
    }
}

/**
 * @brief Disconnect from DoIP server
 * @param tp DoIP client context
 */
void doip_client_disconnect(DoIPClient_t *tp) {
    tp->tcp.close(&tp->tcp);

    doip_change_state(tp, DOIP_STATE_DISCONNECTED);
    tp->rx_offset = 0;
    UDS_LOGI(__FILE__, "DoIP Client: Disconnected");
}

/**
 * @brief Populates SDU info structure for DoIP transport layer.
 *
 * @param hdl Handle to DoIP transport layer
 * @param info Pointer to SDU info structure to populate
 */
void doip_update_sdu_info(const UDSTp_t *hdl, UDSSDU_t *info) {
    if (NULL == info || NULL == hdl) {
        return;
    }

    const DoIPClient_t *impl = (const DoIPClient_t *)hdl;
    info->A_Mtype = UDS_A_MTYPE_DIAG;
    info->A_SA = impl->source_address;
    info->A_TA = impl->target_address;
    info->A_TA_Type = UDS_A_TA_TYPE_PHYSICAL;
    info->A_AE = UDS_TP_NOOP_ADDR;
}

/* --------------------------------------------------------------------------------
 * UDS Transport Layer Interface Functions (send, recv, poll)
 * -------------------------------------------------------------------------------- */

/**
 * @brief Send UDS message via DoIP transport layer.
 *
 * @param hdl Handle to DoIP transport layer
 * @param buf Pointer to buffer containing UDS message
 * @param len Length of UDS message
 * @param info Pointer to SDU info structure (optional)
 * @return ssize_t Number of bytes sent, or negative on error
 */

/* NOTE: SonarCube complains about missing const, but the interface requires non-const */
static ssize_t doip_tp_send(UDSTp_t *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ssize_t ret = -1;
    DoIPClient_t *impl = (DoIPClient_t *)hdl;

    ret = doip_client_send_diag_message(impl, buf, len);
    if (ret < 0) {
        UDS_LOGE(__FILE__, "DoIP TP Send Error");
    } else {
        UDS_LOG_SDU(__FILE__, buf, len, info);
        UDS_LOGD(__FILE__, "DoIP TP Send: Sent %zd bytes", ret);
    }

    // Populate SDU info if provided (physical addressing semantics on DoIP)
    doip_update_sdu_info(hdl, info);
    return ret;
}

/**
 * @brief Receive UDS message via DoIP transport layer.
 *
 * @param hdl Handle to DoIP transport layer
 * @param buf Pointer to buffer to store received UDS message
 * @param bufsize Size of the buffer
 * @param info Pointer to SDU info structure (optional)
 * @return ssize_t Number of bytes received, or negative on error
 */
static ssize_t doip_tp_recv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    UDS_ASSERT(buf);
    DoIPClient_t *impl = (DoIPClient_t *)hdl;

    // Try to receive any pending data (non-blocking poll inside)
    ssize_t rc = doip_receive_data(impl, 0);
    UDS_LOGD(__FILE__, "DoIP TP Recv: doip_receive_data returned %zd", rc);

    // If we have a diagnostic response stored, return it
    if (impl->state == DOIP_STATE_DIAG_MESSAGE_RESPONSE_PENDING && impl->uds_response_len > 0) {
        size_t n = impl->uds_response_len;
        if (n > bufsize) {
            n = bufsize;
        }
        UDS_LOG_SDU(__FILE__, impl->uds_response, n, NULL);
        memcpy(buf, impl->uds_response, n);
        impl->uds_response_len = 0; // consume buffered data
        doip_change_state(impl, DOIP_STATE_READY_FOR_DIAG_REQUEST);

        // Populate SDU info if provided (physical addressing semantics on DoIP)
        doip_update_sdu_info(hdl, info);
        UDS_LOG_SDU(__FILE__, buf, n, info);
        return (ssize_t)n;
    }

    return rc;
}
/**
 * @brief Poll DoIP transport layer status
 * @note Checks if the transport layer is ready to send/receive
 * @return UDS_TP_IDLE if idle, otherwise UDS_TP_SEND_IN_PROGRESS or UDS_TP_RECV_COMPLETE
 */
static UDSTpStatus_t doip_tp_poll(UDSTp_t *hdl) {
    UDS_ASSERT(hdl);
    UDSTpStatus_t status = 0;
    DoIPClient_t *impl = (DoIPClient_t *)hdl;

    // Basic connectivity check
    if (impl->state == DOIP_STATE_DISCONNECTED || impl->tcp.fd < 0) {
        status |= UDS_TP_ERR;
        return status;
    }

    // Pump the socket to process incoming data without blocking
    ssize_t rc = doip_receive_data(impl, 0);
    UDS_LOGV(__FILE__, "DoIP TP Poll: after receive_data rc=%zd", rc);

    if (impl->state == DOIP_STATE_READY_FOR_DIAG_REQUEST) {
        status |= UDS_TP_IDLE;
        return status;
    }

    if (rc < 0) {
        status |= UDS_TP_ERR;
        return status;
    }

    if (impl->state == DOIP_STATE_DIAG_MESSAGE_ACK_PENDING) {
        // 1) If waiting for ACK/NACK, mark send in progress until one arrives.
        if (!impl->diag_ack_received && !impl->diag_nack_received) {
            status |= UDS_TP_SEND_IN_PROGRESS;
        } else if (impl->diag_nack_received) {
            status |= UDS_TP_ERR;
        } else {
            // ACK received; now expect diagnostic response
            status |= UDS_TP_SEND_IN_PROGRESS;
        }
        return status;
    }

    if (impl->state == DOIP_STATE_DIAG_MESSAGE_RESPONSE_PENDING) {

        // 2) If waiting for diagnostic response, indicate completion when data buffered
        if (impl->uds_response_len > 0) {
            status |= UDS_TP_RECV_COMPLETE;
        } else {
            status |= UDS_TP_SEND_IN_PROGRESS; // still waiting on response
        }
        return status;
    }

    // Any other state is considered an error for transport purposes
    status |= UDS_TP_ERR;
    return status;
}

UDSErr_t UDSDoIPInitClient(DoIPClient_t *tp, const char *ipaddress, uint16_t port,
                           uint16_t source_addr, uint16_t target_addr) {
    if (tp == NULL || ipaddress == NULL) {
        return UDS_ERR_INVALID_ARG;
    }

    memset(tp, 0, sizeof(DoIPClient_t));

    doip_change_state(tp, DOIP_STATE_DISCONNECTED);
    tp->source_address = source_addr;
    tp->target_address = target_addr;

    /* Copy server IP address with guaranteed null-termination */
    snprintf(tp->server_ip, sizeof(tp->server_ip), "%s", ipaddress);
    if (tp->server_ip[0] == '\0') {
        UDS_LOGE(__FILE__, "UDS DoIP Client: Invalid server IP address");
        return UDS_ERR_INVALID_ARG;
    }
    tp->server_port = port;

    tp->hdl.send = doip_tp_send;
    tp->hdl.recv = doip_tp_recv;
    tp->hdl.poll = doip_tp_poll;

    memset(&tp->tcp, 0, sizeof(tp->tcp));
    memset(&tp->udp, 0, sizeof(tp->udp));

#ifdef DOIP_MOCK_TP
    /* For testing purposes, use mock transport implementations */
    tp->tcp.init = doip_mock_tcp_init;
    tp->tcp.connect = doip_mock_tcp_connect;
    tp->tcp.send = doip_mock_tcp_send;
    tp->tcp.recv = doip_mock_tcp_recv;
    tp->tcp.close = doip_mock_tcp_close;

    tp->udp.init = doip_mock_udp_init;
    tp->udp.sendto = doip_mock_udp_sendto;
    tp->udp.recv = doip_mock_udp_recv;
    tp->udp.recvfrom = doip_mock_udp_recvfrom;
    tp->udp.close = doip_mock_udp_close;
#else
    /* Use real TCP/UDP transport implementations */
    tp->tcp.init = doip_tp_tcp_init;
    tp->tcp.connect = doip_tp_tcp_connect;
    tp->tcp.send = doip_tp_tcp_send;
    tp->tcp.recv = doip_tp_tcp_recv;
    tp->tcp.close = doip_tp_tcp_close;

    tp->udp.init = doip_tp_udp_init;
    tp->udp.sendto = doip_tp_udp_sendto;
    tp->udp.recv = doip_tp_udp_recv;
    tp->udp.recvfrom = doip_tp_udp_recvfrom;
    tp->udp.close = doip_tp_udp_close;
    tp->udp.join_multicast = doip_tp_udp_join_default_multicast;
#endif

    // sanity checks for transport functions
    if (tp->tcp.init == NULL || tp->tcp.connect == NULL || tp->tcp.send == NULL ||
        tp->tcp.recv == NULL || tp->tcp.close == NULL) {
        UDS_LOGE(__FILE__, "UDS DoIP Client: TCP transport functions not set");
        return UDS_ERR_TPORT;
    }

    if (tp->udp.init == NULL || tp->udp.sendto == NULL || tp->udp.recv == NULL ||
        tp->udp.recvfrom == NULL || tp->udp.close == NULL || tp->udp.join_multicast == NULL) {
        UDS_LOGE(__FILE__, "UDS DoIP Client: UDP transport functions not set");
        return UDS_ERR_TPORT;
    }

    UDS_LOGI(__FILE__, "UDS DoIP Client: Initialized (SA=0x%04X, TA=0x%04X)", tp->source_address,
             tp->target_address);

    if (doip_client_connect(tp)) {
        UDS_LOGE(__FILE__, "UDS DoIP Client: Connect error");
        return UDS_ERR_TPORT;
    }

    if (doip_client_activate_routing(tp)) {
        UDS_LOGE(__FILE__, "UDS DoIP Client: Routing activation error");
        return UDS_ERR_TPORT;
    }

    return UDS_OK;
}

UDSErr_t UDSDoIPActivateRouting(DoIPClient_t *tp) {
    if (tp == NULL) {
        return UDS_ERR_INVALID_ARG;
    }

    if (doip_client_activate_routing(tp)) {
        UDS_LOGE(__FILE__, "UDS DoIP Client: Routing activation error");
        return UDS_ERR_TPORT;
    }
    return UDS_OK;
}

void UDSDoIPDeinit(DoIPClient_t *tp) {
    if (tp == NULL) {
        return;
    }

    doip_client_disconnect(tp);
}

/* --------------------------------------------------------------
 * Selection callback support
 * -------------------------------------------------------------- */
static DoIPSelectServerFn g_select_fn = NULL;
static void *g_select_user = NULL;
static bool g_discovery_request_only = false;
static bool g_discovery_dump_raw = false;

void UDSDoIPSetSelectionCallback(DoIPClient_t *tp, DoIPSelectServerFn fn, void *user) {
    (void)tp; /* per-client not required; use global for simplicity */
    g_select_fn = fn;
    g_select_user = user;
}

void UDSDoIPSetDiscoveryOptions(bool request_only, bool dump_raw) {
    g_discovery_request_only = request_only;
    g_discovery_dump_raw = dump_raw;
}

/* --------------------------------------------------------------
 * UDP discovery: collect responders within timeout, allow selection
 * -------------------------------------------------------------- */
int UDSDoIPDiscoverVehiclesEx(DoIPClient_t *tp, int timeout_ms, bool loopback, uint16_t port) {
    if (!tp)
        return -1;
    tp->udp.loopback = loopback;
    if (tp->udp.init(&tp->udp, port, loopback) < 0) {
        UDS_LOGE(__FILE__, "DoIP UDP: init failed");
        return -1;
    }
    if (!loopback && !g_discovery_request_only) {
        if (tp->udp.join_multicast(&tp->udp) < 0) {
            UDS_LOGE(__FILE__, "DoIP UDP: multicast join failed");
            doip_tp_udp_close(&tp->udp);
            return -1;
        }
    }

    /* Actively send a Vehicle Identification Request */
    {
        uint8_t req[DOIP_HEADER_SIZE];
        req[0] = DOIP_PROTOCOL_VERSION;
        req[1] = DOIP_PROTOCOL_VERSION_INV;
        req[2] = 0x00; /* payload_type MSB: 0x0001 */
        req[3] = 0x01; /* payload_type LSB */
        req[4] = 0x00; /* payload_length: 0 */
        req[5] = 0x00;
        req[6] = 0x00;
        req[7] = 0x00;

        const char *dst_ip = loopback ? "127.0.0.1" : "255.255.255.255";
        uint16_t dst_port = DOIP_UDP_DISCOVERY_PORT;
        ssize_t sent = doip_tp_udp_sendto(&tp->udp, req, sizeof(req), dst_ip, dst_port, 500);
        if (sent <= 0) {
            UDS_LOGW(__FILE__, "DoIP UDP: VI request send failed (dst %s:%u)", dst_ip, dst_port);
        } else {
            UDS_LOGI(__FILE__, "DoIP UDP: sent Vehicle Identification Request to %s:%u", dst_ip,
                     dst_port);
        }
    }

    int found = 0;
    const int slice_ms = 200;
    const int resend_interval_ms = 500;
    int elapsed_ms = 0;
    int sent_count = 1; /* already sent one request above */
    int remaining = timeout_ms > 0 ? timeout_ms : 0;
    uint8_t buf[DOIP_BUFFER_SIZE];
    char src_ip[64];
    uint16_t src_port = 0;

    while (remaining > 0) {
        int win = remaining < slice_ms ? remaining : slice_ms;
        ssize_t n = doip_tp_udp_recvfrom(&tp->udp, buf, sizeof(buf), win, src_ip, sizeof(src_ip),
                                         &src_port);
        if (n < 0) {
            UDS_LOGE(__FILE__, "DoIP UDP: recvfrom error");
            break;
        } else if (n == 0) {
            remaining -= win;
            elapsed_ms += win;
            if (elapsed_ms >= sent_count * resend_interval_ms && found == 0) {
                /* Resend VI request to improve discovery probability */
                uint8_t req2[DOIP_HEADER_SIZE] = {DOIP_PROTOCOL_VERSION,
                                                  DOIP_PROTOCOL_VERSION_INV,
                                                  0x00,
                                                  0x01,
                                                  0x00,
                                                  0x00,
                                                  0x00,
                                                  0x00};
                const char *dst_ip2 = loopback ? "127.0.0.1" : "255.255.255.255";
                uint16_t dst_port2 = DOIP_UDP_DISCOVERY_PORT;
                (void)doip_tp_udp_sendto(&tp->udp, req2, sizeof(req2), dst_ip2, dst_port2, 200);
                UDS_LOGV(__FILE__, "DoIP UDP: re-sent VI request to %s:%u", dst_ip2, dst_port2);
                sent_count++;
            }
            continue;
        }

        found++;
        UDS_LOGI(__FILE__, "DoIP UDP: discovery frame from %s:%u (%zd bytes)", src_ip, src_port, n);
        if (g_discovery_dump_raw) {
            UDS_LOG_SDU(__FILE__, buf, (size_t)n, NULL);
        }

        DoIPDiscoveryInfo info;
        memset(&info, 0, sizeof(info));
        snprintf(info.ip, sizeof(info.ip), "%s", src_ip);
        info.remote_port = src_port;

        /* Parse DoIP header to extract known fields from known payload types */
        if ((size_t)n >= DOIP_HEADER_SIZE) {
            DoIPHeader_t hdr;
            if (doip_header_parse(buf, &hdr)) {
                const uint8_t *pl = buf + DOIP_HEADER_SIZE;
                size_t plen = hdr.payload_length;
                /* Vehicle identification response/announcement payloads commonly start with VIN */
                if (plen >= 17) {
                    memcpy(info.vin, pl, 17);
                    info.vin[17] = '\0';
                }
                /* Next 6 bytes often EID, then 6 bytes GID */
                if (plen >= 23) {
                    for (int i = 0; i < 6; ++i) {
                        sprintf(&info.eid[i * 2], "%02X", pl[17 + i]);
                    }
                    info.eid[12] = '\0';
                }
                if (plen >= 29) {
                    for (int i = 0; i < 6; ++i) {
                        sprintf(&info.gid[i * 2], "%02X", pl[23 + i]);
                    }
                    info.gid[12] = '\0';
                }
                if (info.vin[0] != '\0') {
                    UDS_LOGI(__FILE__, "DoIP UDP: parsed VIN=%s from %s:%u", info.vin, src_ip,
                             src_port);
                }
            } else {
                /* Fallback: coarse VIN scan in entire datagram */
                if ((size_t)n >= 17) {
                    for (size_t i = 0; i + 17 <= (size_t)n; ++i) {
                        bool printable = true;
                        for (size_t j = 0; j < 17; ++j) {
                            uint8_t c = buf[i + j];
                            if (!(c >= '0' && c <= '9') && !(c >= 'A' && c <= 'Z')) {
                                printable = false;
                                break;
                            }
                        }
                        if (printable) {
                            memcpy(info.vin, buf + i, 17);
                            info.vin[17] = '\0';
                            break;
                        }
                    }
                }
                if (info.vin[0] != '\0') {
                    UDS_LOGI(__FILE__, "DoIP UDP: heuristic VIN=%s from %s:%u", info.vin, src_ip,
                             src_port);
                }
            }
        }

        /* Invoke selection callback if provided */
        bool choose = false;
        if (g_select_fn) {
            choose = g_select_fn(&info, g_select_user);
        } else {
            /* Default: choose first responder */
            if (found == 1)
                choose = true;
        }

        if (choose) {
            snprintf(tp->server_ip, sizeof(tp->server_ip), "%s", info.ip);
            tp->server_port = DOIP_TCP_PORT; /* default TCP port */
            UDS_LOGI(__FILE__, "DoIP: selected server %s:%u", tp->server_ip, tp->server_port);
            break; /* stop after selection */
        }

        remaining -= win;
    }

    doip_tp_udp_close(&tp->udp);
    return found;
}

int UDSDoIPDiscoverVehicles(DoIPClient_t *tp, int timeout_ms, bool loopback) {
    /* By default, listen on the tester request port (13401) to receive announcements */
    return UDSDoIPDiscoverVehiclesEx(tp, timeout_ms, loopback,
                                     DOIP_UDP_TEST_EQUIPMENT_REQUEST_PORT);
}

#endif /* UDS_TP_DOIP */