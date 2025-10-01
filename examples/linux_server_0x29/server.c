#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

static UDSServer_t srv;
static UDSTpIsoTpSock_t tp;
static bool done = false;
static uint8_t seed[32] = {0};

// Sample data for RDBI demonstration
static uint8_t sample_vin[18] = "1HGBH41JXMN109186"; // Sample VIN (Vehicle Identification Number)
static uint16_t sample_ecu_serial = 0x1234;          // Sample ECU serial number

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    switch (ev) {
    case UDS_EVT_SecAccessRequestSeed: {
        // TODO: Implement security access seed generation
        printf("Security access seed request received\n");
        return UDS_NRC_ServiceNotSupported;
    }
    case UDS_EVT_SecAccessValidateKey: {
        // TODO: Implement security access key validation
        printf("Security access key validation received\n");
        return UDS_NRC_ServiceNotSupported;
    }
    case UDS_EVT_ReadDataByIdent: {
        UDSRDBIArgs_t *r = (UDSRDBIArgs_t *)arg;
        printf("RDBI request for data identifier: 0x%04X\n", r->dataId);

        switch (r->dataId) {
        case 0xF190: // VIN (Vehicle Identification Number)
            printf("Responding with VIN data\n");
            return r->copy(srv, sample_vin, sizeof(sample_vin));

        case 0xF18C: // ECU Serial Number
            printf("Responding with ECU serial number\n");
            return r->copy(srv, &sample_ecu_serial, sizeof(sample_ecu_serial));

        default:
            printf("Unsupported data identifier: 0x%04X\n", r->dataId);
            return UDS_NRC_RequestOutOfRange;
        }
    }
    default:
        printf("Unhandled event: %d\n", ev);
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
