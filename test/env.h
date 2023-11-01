#ifndef ENV_H
#define ENV_H

#include "iso14229.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void ENV_ServerInit(UDSServer_t *srv);
void ENV_ClientInit(UDSClient_t *client);
void ENV_SessInit(UDSSess_t *sess, const char *name);

#define ENV_SERVER_INIT(srv)                                                                       \
    ENV_ParseOpts(0, NULL);                                                                        \
    ENV_ServerInit(&srv);                                                                          \
    TPMockLogToStdout();

#define ENV_CLIENT_INIT(client)                                                                    \
    ENV_ClientInit(&client);                                                                       \
    TPMockLogToStdout();

#define ENV_SESS_INIT(sess)                                                                        \
    ENV_SessInit(&sess, #sess);                                                                    \
    TPMockLogToStdout();

void ENV_RegisterServer(UDSServer_t *server);
void ENV_RegisterClient(UDSClient_t *client);
void ENV_RegisterSess(UDSSess_t *sess);
void ENV_RunMillis(uint32_t millis);
void ENV_ParseOpts(int argc, char **argv);

#endif
