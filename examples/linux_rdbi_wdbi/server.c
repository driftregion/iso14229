#include "iso14229.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

static UDSServer_t srv;
static UDSTpIsoTpSock_t tp;

static bool done = false;
static uint16_t val_0xf190 = 0;
static uint16_t val_0xf191 = 0;

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

static int fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    switch (ev) {
        case UDS_EVT_ReadDataByIdent: {
            UDSRDBIArgs_t *r = (UDSRDBIArgs_t *)arg;
            printf("received RDBI %04x\n", r->dataId);
            switch (r->dataId) {
                case 0xf190: {
                    return r->copy(srv, &val_0xf190, sizeof(val_0xf190));
                    break;
                }
                case 0xf191: {
                    return r->copy(srv, &val_0xf191, sizeof(val_0xf191));
                    break;
                }
                default:
                    return UDS_NRC_RequestOutOfRange;
            }
        }
        case UDS_EVT_WriteDataByIdent: {
            UDSWDBIArgs_t *r = (UDSWDBIArgs_t *)arg;
            switch (r->dataId) {
                case 0xf190: {
                    if (r->len != sizeof(val_0xf190)) {
                        return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
                    }
                    val_0xf190 = (r->data[0] << 8) + r->data[1];
                    return UDS_PositiveResponse;
                }
                default:
                    return UDS_NRC_RequestOutOfRange;
            }
        }
        default:
            printf("Unhandled event: %s\n", UDSEventToStr(ev));
            return UDS_NRC_ServiceNotSupported;
    }
}

static int sleep_ms(uint32_t tms) {
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
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    if (UDSTpIsoTpSockInitServer(&tp, "vcan0", 0x7E0, 0x7E8, 0x7DF)) {
        fprintf(stderr, "UDSTpIsoTpSockInitServer failed\n");
        exit(-1);
    }

    if (UDSServerInit(&srv)) {
        fprintf(stderr, "UDSServerInit failed\n");
    }

    srv.tp = (UDSTp_t *)&tp;
    srv.fn = fn;

    printf("server up, polling . . .\n");
    while (!done) {
        UDSServerPoll(&srv);
        sleep_ms(1);
    }
    printf("server exiting\n");
    return 0;
}
