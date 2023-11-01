#include "../iso14229.h"
#include "uds_params.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#if UDS_TP == UDS_TP_ISOTP_C
#include "../isotp-c/isotp.h"
#include "isotp-c_on_socketcan.h"
#endif

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg);

static UDSServer_t srv;
static const UDSServerConfig_t cfg = {
    .fn = fn,
#if UDS_TP == UDS_TP_ISOTP_SOCKET
    .if_name = "vcan0",
#endif
    .target_addr = SERVER_FUNC_ID,
    .source_addr = SERVER_PHYS_RECV_ID,
    .source_addr_func = SERVER_FUNC_RECV_ID,
};
static bool serverWantsExit = false;
static struct RWDBIData {
    uint8_t d1;
    int8_t d2;
    uint16_t d3;
    int16_t d4;
} myData = {0};

// 用初始化服务器实例来简单模拟一个ECU复位
// mock an ECU reset by resetting the server
static void mockECUReset(enum UDSECUResetType resetType) {
    printf("Resetting ECU (type: %d)\n", resetType);
    switch (resetType) {
    case kHardReset:
    case kSoftReset:
        UDSServerDeInit(&srv);
        UDSServerInit(&srv, &cfg);
        break;
    default:
        printf("unknown reset type %d\n", resetType);
        break;
    }
}

static uint8_t RDBI(UDSServer_t *srv, UDSRDBIArgs_t *r) {
    static const uint8_t msg[] = "I'm a UDS server    ";
    switch (r->dataId) {
    case 0x1:
        return r->copy(srv, &myData.d1, sizeof(myData.d1));
    case 0x8:
        return r->copy(srv, (void *)msg, sizeof(msg));
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}

static uint8_t WDBI(UDSServer_t *srv, UDSWDBIArgs_t *r) {
    switch (r->dataId) {
    case 0x1:
        if (r->len != sizeof(myData.d1)) {
            return kIncorrectMessageLengthOrInvalidFormat;
        }
        myData.d1 = r->data[0];
        break;
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    switch (ev) {
    case UDS_SRV_EVT_EcuReset: { // 0x10
        UDSECUResetArgs_t *r = (UDSECUResetArgs_t *)arg;
        printf("got ECUReset request of type %x\n", r->type);
        switch (r->type) {
        case kHardReset:
        case kSoftReset:
            return kPositiveResponse;
            break;
        default:
            return kSubFunctionNotSupported;
        }
        break;
    }
    case UDS_SRV_EVT_DiagSessCtrl: { // 0x11
        UDSDiagSessCtrlArgs_t *r = (UDSDiagSessCtrlArgs_t *)arg;
        switch (r->type) {
        case kDefaultSession:
            return kPositiveResponse;
        case kProgrammingSession:
        case kExtendedDiagnostic:
            if (srv->securityLevel > 0) {
                return kPositiveResponse;
            } else {
                return kSecurityAccessDenied;
            }
            break;
        default:
            return kSubFunctionNotSupported;
        }
    }
    case UDS_SRV_EVT_ReadDataByIdent: // 0x22
        return RDBI(srv, (UDSRDBIArgs_t *)arg);
    case UDS_SRV_EVT_SecAccessRequestSeed: { // 0x27
        const uint8_t seed[] = {1, 2, 3, 4};
        UDSSecAccessRequestSeedArgs_t *r = (UDSSecAccessRequestSeedArgs_t *)arg;
        return r->copySeed(srv, seed, sizeof(seed));
    }
    case UDS_SRV_EVT_SecAccessValidateKey: { // 0x27
        return kPositiveResponse;
    }
    case UDS_SRV_EVT_WriteDataByIdent: // 0x2E
        return WDBI(srv, (UDSWDBIArgs_t *)arg);
    case UDS_SRV_EVT_RoutineCtrl: { // 0x31
        UDSRoutineCtrlArgs_t *r = (UDSRoutineCtrlArgs_t *)arg;
        if (RID_TERMINATE_PROCESS == r->id) {
            serverWantsExit = true;
            return kPositiveResponse;
        } else {
            return kRequestOutOfRange;
        }
        break;
    }
    case UDS_SRV_EVT_SessionTimeout:
        printf("server session timed out!\n");
        UDSServerDeInit(srv);
        UDSServerInit(srv, &cfg);
        break;
    case UDS_SRV_EVT_DoScheduledReset:
        printf("powering down!\n");
        mockECUReset(*((enum UDSECUResetType *)arg));
        break;
    default:
        printf("Unhandled event: %d\n", ev);
        return kServiceNotSupported;
    }
    return kGeneralProgrammingFailure;
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
    if (UDSServerInit(&srv, &cfg)) {
        exit(-1);
    }

    printf("server up, polling . . .\n");
    while (!serverWantsExit) {
        UDSServerPoll(&srv);
#if UDS_TP == UDS_TP_ISOTP_C
        SocketCANRecv((UDSTpIsoTpC_t *)srv.tp, cfg.source_addr);
#endif
        SleepMillis(1);
    }
    printf("server exiting\n");
    UDSServerDeInit(&srv);
    return 0;
}
