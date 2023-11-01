#include "../iso14229.h"
#include "uds_params.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#if UDS_TP == UDS_TP_ISOTP_C
#include "../isotp-c/isotp.h"
#include "isotp-c_on_socketcan.h"
#endif

static UDSSeqState_t sendHardReset(UDSClient_t *client) {
    uint8_t resetType = kHardReset;
    printf("%s: sending ECU reset type: %d\n", __func__, resetType);
    UDSSendECUReset(client, resetType);
    return UDSSeqStateGotoNext;
}

static UDSSeqState_t awaitPositiveResponse(UDSClient_t *client) {
    if (client->err) {
        return client->err;
    }
    if (kRequestStateIdle == client->state) {
        printf("got positive response\n");
        return UDSSeqStateGotoNext;
    } else {
        return UDSSeqStateRunning;
    }
}

static UDSSeqState_t awaitResponse(UDSClient_t *client) {
    if (kRequestStateIdle == client->state) {
        printf("got response\n");
        return UDSSeqStateGotoNext;
    } else {
        return UDSSeqStateRunning;
    }
}

static UDSSeqState_t requestSomeData(UDSClient_t *client) {
    printf("%s: calling ReadDataByIdentifier\n", __func__);
    static const uint16_t didList[] = {0x0001, 0x0008};
    UDSSendRDBI(client, didList, 2);
    return UDSSeqStateGotoNext;
}

static UDSSeqState_t printTheData(UDSClient_t *client) {
    uint8_t buf[21];
    uint16_t offset = 0;
    int err;

    printf("Unpacked:\n");
    err = UDSUnpackRDBIResponse(client, 0x0001, buf, DID_0x0001_LEN, &offset);
    if (err) {
        client->err = err;
        return UDSSeqStateDone;
    }
    printf("DID 0x%04x: %d\n", 0x0001, buf[0]);

    err = UDSUnpackRDBIResponse(client, 0x0008, buf, DID_0x0008_LEN, &offset);
    if (err) {
        client->err = err;
        return UDSSeqStateDone;
    }
    printf("DID 0x%04x: %s\n", 0x0008, buf);
    return UDSSeqStateGotoNext;
}

static UDSSeqState_t enterDiagnosticSession(UDSClient_t *client) {
    const uint8_t session = kExtendedDiagnostic;
    UDSSendDiagSessCtrl(client, session);
    printf("%s: entering diagnostic session %d\n", __func__, session);
    return UDSSeqStateGotoNext;
}

static UDSSeqState_t terminateServerProcess(UDSClient_t *client) {
    printf("%s: requesting server shutdown...\n", __func__);
    client->options |= UDS_SUPPRESS_POS_RESP;
    UDSSendRoutineCtrl(client, kStartRoutine, RID_TERMINATE_PROCESS, NULL, 0);
    return UDSSeqStateGotoNext;
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

// clang-format off
/**
 * @brief 流程定义 Sequence Definition
 */
static UDSClientCallback callbacks[] = {
    requestSomeData,
    awaitPositiveResponse,
    printTheData,

    enterDiagnosticSession, 
    awaitResponse,

    // terminateServerProcess,
    NULL,
};
// clang-format on

int main(int ac, char **av) {
    UDSClient_t client;
    UDSClientConfig_t cfg = {
#if UDS_TP == UDS_TP_ISOTP_SOCKET
        .if_name = "vcan0",
#endif
        .source_addr = 0x7E8,
        .target_addr = 0x7E0,
        .target_addr_func = 0x7DF,
    };

    if (UDSClientInit(&client, &cfg)) {
        exit(-1);
    }

    client.cbList = callbacks;
    client.cbIdx = 0;

    printf("running sequence. . .\n");
    int running = 1;
    do {
        running = UDSClientPoll(&client);
#if UDS_TP == UDS_TP_ISOTP_C
        SocketCANRecv((UDSTpISOTpC_t *)client.tp, cfg.source_addr);
#endif
        SleepMillis(1);
    } while (running);

    printf("sequence completed %ld callbacks with client err: %d\n", client.cbIdx, client.err);
    UDSClientDeInit(&client);
    return 0;
}
