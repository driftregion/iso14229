#include "iso14229.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>

static UDSServer_t srv;
static bool done = false;

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    switch (ev) {
    default:
        printf("Unhandled event: %d\n", ev);
        return kServiceNotSupported;
    }
}

static int SleepMillis(uint32_t tms) {
    struct timespec ts;
    int ret;
    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;
    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);
    return ret;
}

int main(int ac, char **av) {
    UDSServerConfig_t cfg = {
        .fn = fn,
#if UDS_TP == UDS_TP_ISOTP_SOCKET
        .if_name = "vcan0",
        .source_addr = 0x7E0,
        .target_addr = 0x7E8,
        .source_addr_func = 0x7DF,
#endif
    };

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    if (UDSServerInit(&srv, &cfg)) {
        exit(-1);
    }

    printf("server up, polling . . .\n");
    while (!done) {
        UDSServerPoll(&srv);
#if UDS_TP == UDS_TP_ISOTP_C
        SocketCANRecv((UDSTpISOTpC_t *)srv.tp, cfg.source_addr);
#endif
        SleepMillis(1);
    }
    printf("server exiting\n");
    UDSServerDeInit(&srv);
    return 0;
}
