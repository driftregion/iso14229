#include "test/env.h"
#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static UDSServer_t *registeredServer = NULL;
static UDSClient_t *registeredClient = NULL;
#define MAX_NUM_TP 16
static UDSTpHandle_t *registeredTps[MAX_NUM_TP];
static unsigned TPCount = 0;
static uint32_t TimeNowMillis = 0;

#define MAX_NUM_HOOKS 16
static struct {
    void (*fn)(void *);
    void *arg;
} Hooks[MAX_NUM_HOOKS] = {0};
static unsigned HookCount;

#define MAX_NUM_TIMEOUT_FN 16
static struct {
    void (*fn)(void *);
    void *arg;
    unsigned timeout;
} TimeoutFns[MAX_NUM_TIMEOUT_FN] = {0};
static unsigned TimeoutFnCnt;

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


// actually sleep for milliseconds
void msleep(int ms) { usleep(ms * 1000); }

static bool IsNetworkedTransport(int tp_type) {
    return tp_type == ENV_TP_TYPE_ISOTP_SOCK || tp_type == ENV_TP_TYPE_ISOTPC;
}

void ENV_AttachHook(void (*fn)(void *), void *arg) {
    assert(HookCount < MAX_NUM_HOOKS);
    Hooks[HookCount].arg = arg;
    Hooks[HookCount].fn = fn;
    HookCount++;
}

void ENV_SetTimeout(void (*fn)(void *), void *arg, unsigned delay) {
    assert(TimeoutFnCnt < MAX_NUM_TIMEOUT_FN);
    TimeoutFns[TimeoutFnCnt].fn = fn;
    TimeoutFns[TimeoutFnCnt].arg = arg;
    TimeoutFns[TimeoutFnCnt].timeout = UDSMillis() + delay;
    TimeoutFnCnt++;
}

void ENV_RunMillis(uint32_t millis) {
    uint32_t end = UDSMillis() + millis;
    while (UDSMillis() < end) {
        if (registeredServer) {
            UDSServerPoll(registeredServer);
        }
        if (registeredClient) {
            UDSErr_t err = UDSClientPoll(registeredClient);
            if (opts.assert_no_client_err) {
                assert_int_equal(err, UDS_OK);
            }
        }
        for (unsigned i = 0; i < TPCount; i++) {
            UDSTpPoll(registeredTps[i]);
        }
        for (unsigned i = 0; i < HookCount; i++) {
            Hooks[i].fn(Hooks[i].arg);
        }
        for (unsigned i = 0; i < TimeoutFnCnt; i++) {
            if (UDSMillis() == TimeoutFns[i].timeout) {
                TimeoutFns[i].fn(TimeoutFns[i].arg);
            }
        }
        TimeNowMillis++;

        // uses vcan, needs delay
        if (IsNetworkedTransport(opts.tp_type)) {
            // usleep(10);
            msleep(1);
        }
    }
}

