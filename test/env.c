#include "test/env.h"
#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tp/isotp_c_socketcan.h"
#include "tp/mock.h"
#include "tp/isotp_sock.h"

static UDSServer_t *registeredServer = NULL;
static UDSClient_t *registeredClient = NULL;
#define MAX_NUM_TP 8
static UDSTpHandle_t *registeredTps[MAX_NUM_TP];
static unsigned TPCount = 0;
static uint32_t TimeNowMillis = 0;

static ENV_Opts_t opts = {
    .tp_type = ENV_TP_TYPE_MOCK,
    .ifname = "vcan0",
    .srv_src_addr = 0x7E8,
    .srv_target_addr = 0x7E0,
    .srv_src_addr_func = 0x7DF,
    .cli_src_addr = 0x7E0,
    .cli_target_addr = 0x7E8,
    .cli_tgt_addr_func = 0x7DF,
};

static const char *parse_env_var(const char *name, const char *default_val) {
    const char *val = getenv(name);
    if (val) {
        return val;
    }
    return default_val;
}

static const int parse_int_env_var(const char *name, const int default_val) {
    const char *val = getenv(name);
    if (val) {
        return atoi(val);
    }
    return default_val;
}

static void ENV_ParseOpts() {
    opts.ifname = parse_env_var("UDS_IFNAME", opts.ifname);
    opts.tp_type = parse_int_env_var("UDS_TP_TYPE", opts.tp_type);
    opts.srv_src_addr = parse_int_env_var("UDS_SRV_SRC_ADDR", opts.srv_src_addr);
    opts.srv_target_addr = parse_int_env_var("UDS_SRV_TARGET_ADDR", opts.srv_target_addr);
    opts.srv_src_addr_func = parse_int_env_var("UDS_SRV_SRC_ADDR_FUNC", opts.srv_src_addr_func);
    opts.cli_src_addr = parse_int_env_var("UDS_CLI_SRC_ADDR", opts.cli_src_addr);
    opts.cli_target_addr = parse_int_env_var("UDS_CLI_TARGET_ADDR", opts.cli_target_addr);
    opts.cli_tgt_addr_func = parse_int_env_var("UDS_CLI_TGT_ADDR_FUNC", opts.cli_tgt_addr_func);
}

void ENV_ServerInit(UDSServer_t *srv) {
    ENV_ParseOpts();
    UDSServerInit(srv);
    srv->tp = ENV_TpNew("server");
    ENV_RegisterServer(srv);
}

void ENV_ClientInit(UDSClient_t *cli) {
    ENV_ParseOpts();
    UDSClientInit(cli);
    cli->tp = ENV_TpNew("client");
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
    return tp_type == ENV_TP_TYPE_ISOTP_SOCK || tp_type == ENV_TP_TYPE_ISOTPC;
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
        if (IsNetworkedTransport(opts.tp_type)) {
            // usleep(10);
            msleep(1);
        }
    }
}


UDSTpHandle_t *ENV_TpNew(const char *name) {
    ENV_ParseOpts();
    UDSTpHandle_t *tp = NULL;
    if (0 == strcasecmp(name, "server"))
        switch (opts.tp_type) {
        case ENV_TP_TYPE_MOCK:
            tp = TPMockNew("server", TPMOCK_DEFAULT_SERVER_ARGS);
            break;
        case ENV_TP_TYPE_ISOTP_SOCK: {
            UDSTpIsoTpSock_t *isotp = malloc(sizeof(UDSTpIsoTpSock_t));
            strcpy(isotp->tag, "server");
            assert(UDS_OK == UDSTpIsoTpSockInitServer(isotp, opts.ifname,
                                                    opts.srv_src_addr, opts.srv_target_addr,
                                                    opts.srv_src_addr_func));
            tp = (UDSTpHandle_t *)isotp;
            break;
        }
        case ENV_TP_TYPE_ISOTPC: {
            UDSTpISOTpC_t *isotp = malloc(sizeof(UDSTpISOTpC_t));
            strcpy(isotp->tag, "server");
 
            assert(UDS_OK == UDSTpISOTpCInit(isotp, opts.ifname,
                                                    opts.srv_src_addr, opts.srv_target_addr,
                                                    opts.srv_src_addr_func, 0));
            tp = (UDSTpHandle_t *)isotp;
            break;
        }
        default:
            printf("unknown TP type: %d\n", opts.tp_type);
            return NULL;
        }
    else if (0 == strcasecmp(name, "client")) {
        switch (opts.tp_type) {
        case ENV_TP_TYPE_MOCK:
            tp = TPMockNew("client", TPMOCK_DEFAULT_CLIENT_ARGS);
            break;
        case ENV_TP_TYPE_ISOTP_SOCK: {
            UDSTpIsoTpSock_t *isotp = malloc(sizeof(UDSTpIsoTpSock_t));
            strcpy(isotp->tag, "client");
            assert(UDS_OK == UDSTpIsoTpSockInitClient(isotp, opts.ifname,
                                                    opts.cli_src_addr, opts.cli_target_addr,
                                                    opts.cli_tgt_addr_func));
            tp = (UDSTpHandle_t *)isotp;
            break;
        }
        case ENV_TP_TYPE_ISOTPC: {
            UDSTpISOTpC_t *isotp = malloc(sizeof(UDSTpISOTpC_t));
            strcpy(isotp->tag, "client");
            assert(UDS_OK == UDSTpISOTpCInit(isotp, opts.ifname,
                                                    opts.cli_src_addr, opts.cli_target_addr, 0,
                                                    opts.cli_tgt_addr_func));
            tp = (UDSTpHandle_t *)isotp;
            break;
        }
        default:
            printf("unknown TP type: %d\n", opts.tp_type);
            return NULL;
        }
    }
    else {
        printf("unknown mock tp: %s\n", name);
        return NULL;
    }
    registeredTps[TPCount++] = tp;
    return tp;
}

void ENV_TpFree(UDSTpHandle_t *tp) {
    switch (opts.tp_type) {
        case ENV_TP_TYPE_MOCK:
            TPMockFree(tp);
            break;
        case ENV_TP_TYPE_ISOTP_SOCK:
            UDSTpIsoTpSockDeinit((UDSTpIsoTpSock_t *)tp);
            break;
        case ENV_TP_TYPE_ISOTPC:
            UDSTpISOTpCDeinit((UDSTpISOTpC_t *)tp);
            free(tp);
            break;
    }
}

const ENV_Opts_t *ENV_GetOpts() { return &opts; }