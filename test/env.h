#ifndef ENV_H
#define ENV_H

#include "iso14229.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum ENV_TpType {
    ENV_TP_TYPE_MOCK,       // tp/mock.h
    ENV_TP_TYPE_ISOTP_SOCK, // tp/isotp_sock.h
    ENV_TP_TYPE_ISOTPC,     // tp/isotp_c_socketcan.h
};

/**
 * @brief Environment options
 */
typedef struct {
    enum ENV_TpType tp_type; // transport type
    const char *ifname;      // CAN interface name used for isotp_sock and socketcan
    uint32_t srv_src_addr, srv_target_addr, srv_src_addr_func;
    uint32_t cli_src_addr, cli_target_addr, cli_tgt_addr_func;
} ENV_Opts_t;

void ENV_ServerInit(UDSServer_t *srv);
void ENV_ClientInit(UDSClient_t *client);

#define ENV_SERVER_INIT(srv)                                                                       \
    ENV_ServerInit(&srv);                                                                          \
    TPMockLogToStdout();

#define ENV_CLIENT_INIT(client)                                                                    \
    ENV_ClientInit(&client);                                                                       \
    TPMockLogToStdout();

/**
 * @brief return a transport configured as client
 * @return UDSTpHandle_t*
 */
UDSTpHandle_t *ENV_TpNew(const char *name);
void ENV_TpFree(UDSTpHandle_t *tp);
void ENV_RegisterServer(UDSServer_t *server);
void ENV_RegisterClient(UDSClient_t *client);
void ENV_RunMillis(uint32_t millis);
void ENV_RunMillisForTpRegisteredAt(uint32_t millis, unsigned at);
const ENV_Opts_t *ENV_GetOpts();

#endif
