#include "test/env.h"
#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static uint32_t TimeNowMillis = 0;

uint32_t UDSMillis(void) { return TimeNowMillis; }

struct ScheduledEvent {
    uint32_t time_ms;
    struct Behavior *b;
};

struct MockServerImpl {
    struct Behavior behaviors[16];
    int num_behaviors;
    struct ScheduledEvent events[16];
    int num_events;
};

MockServer_t *MockServerNew(void) {
    MockServer_t *srv = malloc(sizeof(MockServer_t));
    memset(srv, 0, sizeof(MockServer_t));
    srv->impl = malloc(sizeof(struct MockServerImpl));
    memset(srv->impl, 0, sizeof(struct MockServerImpl));
    return srv;
}

void MockServerFree(MockServer_t *srv) {
    free(srv->impl);
    free(srv);
}

void MockServerAddBehavior(MockServer_t *srv, struct Behavior *b) {
    // a behavior string takes the form:  "rx=0x11,0x22; tx=0x00,0x33,0x44; delay_ms=200"
    // arguments are separated by semicolons
    // bytes are separated by commas. Parse the bytes 
    struct MockServerImpl *impl = srv->impl;
    struct Behavior *new_b = &impl->behaviors[impl->num_behaviors++];
    memcpy(new_b, b, sizeof(struct Behavior));
    UDS_LOGI(__FILE__, "added behavior with %d ms delay", b->delay_ms);
}

void MockServerPoll(MockServer_t *srv) {
    assert(srv->tp);
    struct MockServerImpl *impl = srv->impl;
    UDSTpPoll(srv->tp);

    if (UDSTpGetRecvLen(srv->tp)) { 
        UDSTp_t *tp = srv->tp;
        UDSSDU_t info;
        uint8_t *buf = NULL;
        size_t recv_len = UDSTpPeek(tp, &buf, &info);
        if (recv_len > 0) {
            for (int i = 0; i < impl->num_behaviors; i++) {
                struct Behavior *b = &impl->behaviors[i];
                if (recv_len == b->req_len && memcmp(buf, b->req_data, recv_len) == 0) {
                    struct ScheduledEvent *evt = &impl->events[impl->num_events++];
                    evt->time_ms = UDSMillis() + b->delay_ms;
                    evt->b = b;
                    UDS_LOGI(__FILE__, "scheduled event in %d ms", b->delay_ms);
                    break;
                }
            }
        }
        UDSTpAckRecv(srv->tp);
    }

    for (int i = 0; i < impl->num_events; i++) {
        struct ScheduledEvent *evt = &impl->events[i];
        if (UDSMillis() >= evt->time_ms) {
            UDSTpSend(srv->tp, evt->b->resp_data, evt->b->resp_len, NULL);
            for (int j = i + 1; j < impl->num_events; j++) {
                impl->events[j - 1] = impl->events[j];
            }
            impl->num_events--;
            i--;
        }
    }

}

void EnvRunMillis(Env_t *env, uint32_t millis) {
    uint32_t end = UDSMillis() + millis;
    while (UDSMillis() < end) {
        if (env->do_not_poll) {
            ;
        } else {
            if (env->server) {
                UDSServerPoll(env->server);
            } 
            if (env->server_tp) {
                UDSTpPoll(env->server_tp);
            }
            if (env->client) {
                UDSClientPoll(env->client);
            } 
            if (env->client_tp) {
                UDSTpPoll(env->client_tp);
            }
            if (env->mock_server) { 
                MockServerPoll(env->mock_server);
            }
        }
        if (env->is_real_time) {
            usleep(1000);
        }
        TimeNowMillis++;
    }
}

