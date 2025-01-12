#include "test/env.h"
#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t TimeNowMillis = 0;

uint32_t UDSMillis(void) { return TimeNowMillis; }

void EnvRunMillis(Env_t *env, uint32_t millis) {
    uint32_t end = UDSMillis() + millis;
    while (UDSMillis() < end) {
        if (env->server) {
            UDSServerPoll(env->server);
        } else if (env->server_tp) {
            UDSTpPoll(env->server_tp);
        }
        if (env->client) {
            UDSClientPoll(env->client);
        } else if (env->client_tp) {
            UDSTpPoll(env->client_tp);
        }
        if (env->is_real_time) {
            usleep(1000);
        }
        TimeNowMillis++;
    }
}

