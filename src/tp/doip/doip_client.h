#if defined(UDS_TP_DOIP)

#pragma once
#include "tp.h"
#include "uds.h"
#include "doip_defines.h"
#include "doip_transport.h"


#define DOIP_ACK_TIMEOUT_MS 1000 /* 1 second for Diagnostic ACK (0x8002 or 0x8003)*/

/* DoIP Client State */
typedef enum {
    DOIP_STATE_DISCONNECTED,
    DOIP_STATE_CONNECTED,
    DOIP_STATE_ROUTING_ACTIVATION_PENDING,
    DOIP_STATE_READY_FOR_DIAG_REQUEST,
    // Diag message states for tracking ACK/NACK and responses
    DOIP_STATE_DIAG_MESSAGE_SEND_PENDING,
    DOIP_STATE_DIAG_MESSAGE_ACK_PENDING,
    DOIP_STATE_DIAG_MESSAGE_RESPONSE_PENDING,
    DOIP_STATE_ERROR
} DoIPClientState_t;

/* DoIP Client Context */
typedef struct {
    UDSTp_t hdl;    /* Must be the first entry! */
    DoIPTransport tcp;        /* TCP transport for diagnostics */
    DoIPTransport udp;        /* UDP transport for discovery */
    DoIPClientState_t state;

    uint16_t source_address;        /* Client logical address */
    uint16_t target_address;        /* Server logical address */

    char server_ip[64];
    uint16_t server_port;
    bool udp_loopback;        /* discovery via loopback instead of multicast */

    uint8_t rx_buffer[DOIP_BUFFER_SIZE];  /* Raw socket receive buffer */
    size_t rx_offset;

    uint8_t uds_response[DOIP_BUFFER_SIZE]; /* Processed UDS response data */
    size_t uds_response_len;

    bool routing_activated;
    bool diag_ack_received;
    bool diag_nack_received;
    uint8_t diag_nack_code;
} DoIPClient_t;

/**
 * @brief Initialize DoIP client transport layer
 *
 * @param tp Pointer to DoIP client context
 * @param ipaddress Server IP address as a string
 * @param port Server port number
 * @param source_addr Client logical address (range 0x0E00 - 0x0FFF)
 * @param target_addr Server logical address
 * @return UDSErr_t UDS_OK on success, error code otherwise
 */
UDSErr_t UDSDoIPInitClient(DoIPClient_t *tp, const char *ipaddress, uint16_t port, uint16_t source_addr, uint16_t target_addr);

/**
 * @brief Deinitialize DoIP client transport layer
 *
 * @param tp Pointer to DoIP client context
 */
void UDSDoIPDeinit(DoIPClient_t *tp);

/**
 * @brief Discover vehicles using UDP DoIP
 * @param tp DoIP client context
 * @param timeout_ms Receive timeout in ms
 * @param loopback If true, use loopback instead of multicast
 * @return number of discovery frames observed (>=0), or negative on error
 */
int UDSDoIPDiscoverVehicles(DoIPClient_t *tp, int timeout_ms, bool loopback);

#endif
