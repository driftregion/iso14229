#include "test/env.h"
#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tp/mock.h"
#include "tp/isotp_sock.h"

static UDSServer_t *registeredServer = NULL;
static UDSClient_t *registeredClient = NULL;
static UDSSess_t *registeredSess = NULL;
static uint32_t TimeNowMillis = 0;
typedef struct {

} ENV_t;

struct IntOpt {
    const char *name;
    int *val;
};

enum {
    OPTS_TP_TYPE_MOCK,
    OPTS_TP_TYPE_ISOTP_SOCK,
};

typedef struct {
    int tp_type;
    char ifname[32];
    uint32_t srv_src_addr, srv_target_addr, srv_src_addr_func;
    uint32_t cli_src_addr, cli_target_addr, cli_tgt_addr_func;
} Cfg_t;

static Cfg_t cfg;

// static struct IntOpt IntOpts[] = {
//     {"TP_TYPE", &opts.tp_type},
// };

void ENV_ServerInit(UDSServer_t *srv) {
    UDSTpHandle_t *tp = NULL;
    switch (cfg.tp_type) {
    case OPTS_TP_TYPE_MOCK:
        tp = TPMockCreate("server");
        break;
    case OPTS_TP_TYPE_ISOTP_SOCK: {
        UDSTpIsoTpSock_t *isotp = malloc(sizeof(UDSTpIsoTpSock_t));
        strcpy(isotp->tag, "server");
        assert(UDS_OK == UDSTpIsoTpSockInitServer(isotp, cfg.ifname,
                                                  cfg.srv_src_addr, cfg.srv_target_addr,
                                                  cfg.srv_src_addr_func));
        tp = (UDSTpHandle_t *)isotp;
        break;
    }
    default:
        printf("unknown TP type: %d\n", cfg.tp_type);
        exit(1);
    }

    UDSServerInit(srv, &(UDSServerConfig_t){
                           .fn = NULL,
                           .tp = tp,
                           .source_addr = 0x7E8,
                           .target_addr = 0x7E0,
                           .source_addr_func = 0x7DF,
                       });
    ENV_RegisterServer(srv);
}

void ENV_ClientInit(UDSClient_t *cli) {
    UDSTpHandle_t *tp = NULL;
    switch (cfg.tp_type) {
    case OPTS_TP_TYPE_MOCK:
        tp = TPMockCreate("client");
        break;
    case OPTS_TP_TYPE_ISOTP_SOCK: {
        UDSTpIsoTpSock_t *isotp = malloc(sizeof(UDSTpIsoTpSock_t));
        strcpy(isotp->tag, "client");
        assert(UDS_OK == UDSTpIsoTpSockInitClient(isotp, cfg.ifname,
                                                  cfg.cli_src_addr, cfg.cli_target_addr,
                                                  cfg.cli_tgt_addr_func));
        tp = (UDSTpHandle_t *)isotp;
        break;
    }
    default:
        printf("unknown TP type: %d\n", cfg.tp_type);
        exit(1);
    }

    UDSClientInit(cli, &(UDSClientConfig_t){
                           .tp = tp,
                           .source_addr = 0x7E0,
                           .target_addr = 0x7E8,
                           .target_addr_func = 0x7DF,
                       });
    ENV_RegisterClient(cli);
}

void ENV_SessInit(UDSSess_t *sess, const char *name) {
    UDSTpHandle_t *tp = NULL;
    switch (cfg.tp_type) {
    case OPTS_TP_TYPE_MOCK:
        tp = TPMockCreate(name);
        break;
    case OPTS_TP_TYPE_ISOTP_SOCK: {
        UDSTpIsoTpSock_t *isotp = malloc(sizeof(UDSTpIsoTpSock_t));
        strncpy(isotp->tag, name, sizeof(isotp->tag));
        assert(UDS_OK == UDSTpIsoTpSockInitClient(isotp, cfg.ifname,
                                                  cfg.cli_src_addr, cfg.cli_target_addr,
                                                  cfg.cli_tgt_addr_func));
        tp = (UDSTpHandle_t *)isotp;
        break;
    }
    default:
        printf("unknown TP type: %d\n", cfg.tp_type);
        exit(1);
    }

    UDSSessInit(sess, &(UDSSessConfig_t){
                          .tp = tp,
                          .source_addr = 0x7E0,
                          .target_addr = 0x7E8,
                          .target_addr_func = 0x7DF,
                      });
    ENV_RegisterSess(sess);
}

void ENV_RegisterServer(UDSServer_t *server) { registeredServer = server; }

void ENV_RegisterClient(UDSClient_t *client) { registeredClient = client; }

void ENV_RegisterSess(UDSSess_t *sess) { registeredSess = sess; }

uint32_t UDSMillis() { return TimeNowMillis; }

// actually sleep for milliseconds
void msleep(int ms) { 
    usleep(ms * 1000);
}

void ENV_RunMillis(uint32_t millis) {
    uint32_t end = UDSMillis() + millis;
    while (UDSMillis() < end) {
        if (registeredServer) {
            UDSServerPoll(registeredServer);
        }
        if (registeredClient) {
            UDSClientPoll(registeredClient);
        }
        if (registeredSess) {
            UDSSessPoll(registeredSess);
        }
        TimeNowMillis++;

        // uses vcan, needs delay
        if (cfg.tp_type == OPTS_TP_TYPE_ISOTP_SOCK) {
            usleep(10);
            // msleep(1);
        }
    }
}

struct opts {
    const char *arg;
};


void ENV_ParseOpts(int argc, char **argv) {
    snprintf(cfg.ifname, sizeof(cfg.ifname), "vcan0");
    cfg.srv_src_addr = 0x7E8;
    cfg.srv_target_addr = 0x7E0;
    cfg.srv_src_addr_func = 0x7DF;
    cfg.cli_src_addr = 0x7E0;
    cfg.cli_target_addr = 0x7E8;
    cfg.cli_tgt_addr_func = 0x7DF;

    const char *tp = getenv("TP");
    if (0 == strcasecmp(tp, "mock")) {
        cfg.tp_type = OPTS_TP_TYPE_MOCK;
    } else if (0 == strcasecmp(tp, "isotp_sock")) {
        cfg.tp_type = OPTS_TP_TYPE_ISOTP_SOCK;
    } else {
        printf("unknown TP: %s\n", tp);
        exit(1);
    }
}