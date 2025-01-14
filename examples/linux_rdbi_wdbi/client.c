#include "iso14229.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>


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

int step = 0;
UDSErr_t err = UDS_OK;

typedef struct {
    uint16_t did;
    uint16_t len;
    void *data;
    void *(*UnpackFn)(void *dst, const void *src, size_t n);
} UDSRDBIVar_t;

UDSErr_t UDSUnpackRDBIResponse2(UDSClient_t *client, UDSRDBIVar_t *vars) {
    uint16_t offset = UDS_0X22_RESP_BASE_LEN;
    if (client == NULL || vars == NULL) {
        return UDS_ERR_INVALID_ARG;
    }
    while (vars->data && vars->UnpackFn) {
        if (offset + sizeof(uint16_t) > client->recv_size) {
            return UDS_ERR_RESP_TOO_SHORT;
        }

        uint16_t did = (client->recv_buf[offset] << 8) + client->recv_buf[offset + 1];
        if (did != vars->did) {
            return UDS_ERR_DID_MISMATCH;
        }

        if (offset + sizeof(uint16_t) + vars->len > client->recv_size) {
            return UDS_ERR_RESP_TOO_SHORT;
        }

        vars->UnpackFn(vars->data, client->recv_buf + offset + sizeof(uint16_t), vars->len);

        offset += sizeof(uint16_t) + vars->len;
        vars++;
    }
    return UDS_OK;
}


UDSErr_t fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    if (evt != UDS_EVT_Poll) {
        printf("%06d: %s (%d) \n", UDSMillis(), UDSEvtToStr(evt), evt);
    }
    if (UDS_EVT_Err == evt) {
        printf("Exiting on step %d with error: %s\n", step, UDSErrToStr(*(UDSErr_t*)ev_data));
        err = *(UDSErr_t *)ev_data;
        step = -1;
    }
    switch (step) {
        case 0: {
            uint16_t f190, f191;
            UDSRDBIVar_t vars[] = {
                {0xf190, 2, &f190, memcpy},
                {0xf191, 2, &f191, memcpy},
                {0, 0, NULL, NULL}
            };
            switch (evt) {
                case UDS_EVT_Idle: {
                    const uint16_t dids[] = {0xf190, 0xf191};
                    err = UDSSendRDBI(client, dids, 2);
                    if (err) {
                        printf("UDSSendRDBI failed with err: %d\n", err);
                        step = -1;
                    }
                    break;
                    }
                case UDS_EVT_ResponseReceived: {
                    err = UDSUnpackRDBIResponse2(client, vars);
                    if (err) {
                        printf("UDSUnpackRDBIResponse failed with err: %s\n", UDSErrToStr(err));
                        step = -1;
                    }
                    printf("0xf190 has value %d\n", f190);
                    printf("0xf191 has value %d\n", f191);
                    step = 1;
                    break;
                }
                default:
                    break;
            }
        break;
        }
        case 1: {
            switch (evt) {
                case UDS_EVT_Idle: {
                    uint16_t val = 10;
                    uint8_t data[2] = {
                        (val & 0xff00) >> 8,
                        val & 0x00ff,
                    };
                    UDSSendWDBI(client, 0xf190, data, sizeof(data));
                    break;
                }
                case UDS_EVT_ResponseReceived: {
                    step = -1;
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
    return UDS_OK;
}

int main(int ac, char **av) {
    UDSClient_t client;
    UDSTpIsoTpSock_t tp;

    if (UDSTpIsoTpSockInitClient(&tp, "vcan0", 0x7E8, 0x7E0, 0x7DF)) {
        fprintf(stderr, "UDSTpIsoTpSockInitClient failed\n");
        exit(-1);
    }

    if (UDSClientInit(&client)) {
        exit(-1);
    }

    client.tp = (UDSTp_t *)&tp;
    client.fn = fn;

    printf("polling\n");
    while (step >= 0) {
        UDSClientPoll(&client);
    }

    return err;
}
