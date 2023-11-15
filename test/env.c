#include "test/env.h"
#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tp/mock.h"
#include "tp/isotp_sock.h"
// #include "tp/isotp_c_socketcan.h"

static UDSServer_t *registeredServer = NULL;
static UDSClient_t *registeredClient = NULL;
#define MAX_NUM_TP 8
static UDSTpHandle_t *registeredTps[MAX_NUM_TP];
static unsigned TPCount = 0;

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
    OPTS_TP_TYPE_ISOTPC,
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
        tp = TPMockCreate("server", TPMOCK_DEFAULT_SERVER_ARGS);
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
    case OPTS_TP_TYPE_ISOTPC: {
        // UDSTpISOTpC_t *isotp = malloc(sizeof(UDSTpISOTpC_t));
        // strcpy(isotp->tag, "server");
        // assert(UDS_OK == UDSTpISOTpCInitServer(isotp, srv, cfg.ifname,
        //                                         cfg.srv_src_addr, cfg.srv_target_addr,
        //                                         cfg.srv_src_addr_func));
        // tp = (UDSTpHandle_t *)isotp;
        break;
    }
    default:
        printf("unknown TP type: %d\n", cfg.tp_type);
        exit(1);
    }

    UDSServerInit(srv);
    srv->tp = tp;
    ENV_RegisterServer(srv);
}

void ENV_ClientInit(UDSClient_t *cli) {
    UDSTpHandle_t *tp = NULL;
    switch (cfg.tp_type) {
    case OPTS_TP_TYPE_MOCK:
        tp = TPMockCreate("client", TPMOCK_DEFAULT_CLIENT_ARGS);
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
    case OPTS_TP_TYPE_ISOTPC: {
        // UDSTpISOTpC_t *isotp = malloc(sizeof(UDSTpISOTpC_t));
        // strcpy(isotp->tag, "client");
        // assert(UDS_OK == UDSTpISOTpCInitClient(isotp, cli, cfg.ifname,
        //                                         cfg.cli_src_addr, cfg.cli_target_addr,
        //                                         cfg.cli_tgt_addr_func));
        // tp = (UDSTpHandle_t *)isotp;
        // break;
    }
    default:
        printf("unknown TP type: %d\n", cfg.tp_type);
        exit(1);
    }

    UDSClientInit(cli);
    cli->tp = tp;
    ENV_RegisterClient(cli);
}


void ENV_RegisterServer(UDSServer_t *server) { registeredServer = server; }

void ENV_RegisterClient(UDSClient_t *client) { registeredClient = client; }

uint32_t UDSMillis() { return TimeNowMillis; }

// actually sleep for milliseconds
void msleep(int ms) { 
    usleep(ms * 1000);
}

static bool IsNetworkedTransport(int tp_type) {
    return tp_type == OPTS_TP_TYPE_ISOTP_SOCK || tp_type == OPTS_TP_TYPE_ISOTPC;
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
        for (unsigned i = 0; i < TPCount; i++) {
            UDSTpPoll(registeredTps[i]);
        }
        TimeNowMillis++;

        // uses vcan, needs delay
        if (IsNetworkedTransport(cfg.tp_type)) {
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
    if (tp == NULL) {
        tp = "mock";
    }
    if (0 == strcasecmp(tp, "mock")) {
        cfg.tp_type = OPTS_TP_TYPE_MOCK;
    } else if (0 == strcasecmp(tp, "isotp_sock")) {
        cfg.tp_type = OPTS_TP_TYPE_ISOTP_SOCK;
    } else if (0 == strcasecmp(tp, "isotp-c")) {
        cfg.tp_type = OPTS_TP_TYPE_ISOTPC;
    } else {
        printf("unknown TP: %s\n", tp);
        exit(1);
    }
}


UDSTpHandle_t *ENV_GetMockTp(const char *name) {
    UDSTpHandle_t *tp = NULL;
    if (0 == strcasecmp(name, "server"))
        tp = TPMockCreate(name, TPMOCK_DEFAULT_SERVER_ARGS);
    else if (0 == strcasecmp(name, "client")) {
        tp = TPMockCreate(name, TPMOCK_DEFAULT_CLIENT_ARGS);
    }
    else {
        printf("unknown mock tp: %s\n", name);
        return NULL;
    }
    registeredTps[TPCount++] = tp;
    return tp;
}