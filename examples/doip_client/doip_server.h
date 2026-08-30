/**
 * @file doip_server.h
 * @brief ISO 13400 (DoIP) Transport Layer - Server API
 */

#ifndef DOIP_SERVER_H
#define DOIP_SERVER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize DoIP server
 * 
 * @param logical_address Server's DoIP logical address
 * @param diag_msg_callback Callback function for received diagnostic messages
 * @return 0 on success, -1 on error
 */
int doip_server_init(uint16_t logical_address,
                     void (*diag_msg_callback)(uint16_t source_addr, 
                                              const uint8_t *data, 
                                              size_t len));

/**
 * @brief Send diagnostic message response to client
 * 
 * @param source_address Client's source address (routing activated)
 * @param data UDS response data
 * @param len Length of response data
 * @return 0 on success, -1 on error
 */
int doip_server_send_diag_response(uint16_t source_address,
                                   const uint8_t *data, 
                                   size_t len);

/**
 * @brief Process DoIP server events
 * 
 * This function should be called periodically to handle incoming connections
 * and messages. It will block for up to timeout_ms waiting for events.
 * 
 * @param timeout_ms Timeout in milliseconds
 */
void doip_server_process(int timeout_ms);

/**
 * @brief Shutdown DoIP server
 * 
 * Closes all connections and releases resources.
 */
void doip_server_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* DOIP_SERVER_H */
