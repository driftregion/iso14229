#include "test/env.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

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
    struct MockServerImpl *impl = srv->impl;
    struct Behavior *new_b = &impl->behaviors[impl->num_behaviors++];
    memcpy(new_b, b, sizeof(struct Behavior));
    switch (b->tag) {
        case ExactRequestResponse:
            UDS_LOGI(__FILE__, "added behavior ExactRequestResponse with %d ms delay", b->exact_request_response.delay_ms);
            break;
        case TimedSend:
            struct ScheduledEvent *evt = &impl->events[impl->num_events++];
            evt->b = b;
            evt->time_ms = b->timed_send.when;
            UDS_LOGI(__FILE__, "added behavior TimedSend");
            break;
        default:
            break;
    }
}

void MockServerPoll(MockServer_t *srv) {
    assert(srv->tp);
    struct MockServerImpl *impl = srv->impl;
    UDSTpPoll(srv->tp);
    uint8_t buf[UDS_TP_MTU] = {0};
    UDSSDU_t info = {0};
    ssize_t len = UDSTpRecv(srv->tp, buf, sizeof(buf), &info);
    UDS_ASSERT(len >= 0);
    size_t recv_len = (size_t)len;
    if (recv_len > 0) {
        for (int i = 0; i < impl->num_behaviors; i++) {
            struct Behavior *b = &impl->behaviors[i];
            switch (b->tag) {
                case ExactRequestResponse:
                    struct ExactRequestResponse *s = &b->exact_request_response;
                    if (recv_len == s->req_len && memcmp(buf, s->req_data, recv_len) == 0) {
                        struct ScheduledEvent *evt = &impl->events[impl->num_events++];
                        evt->time_ms = UDSMillis() + s->delay_ms;
                        evt->b = b;
                        UDS_LOGI(__FILE__, "scheduled event in %d ms", s->delay_ms);
                        break;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    for (int i = 0; i < impl->num_events; i++) {
        struct ScheduledEvent *evt = &impl->events[i];
        if (UDSMillis() >= evt->time_ms) {
            switch (evt->b->tag) {
                case ExactRequestResponse: {
                    struct ExactRequestResponse *s = &evt->b->exact_request_response;
                    UDSTpSend(srv->tp, s->resp_data, s->resp_len, NULL);
                    break;
                }
                case TimedSend: {
                    struct TimedSend *s = &evt->b->timed_send;
                    UDSTpSend(srv->tp, s->send_data, s->send_len, NULL);
                    break;
                }
                default:
                    break;
            }

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
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
        }
        TimeNowMillis++;
    }
}

