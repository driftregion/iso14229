#define UDS_DBG_PRINT printf
#include "../iso14229.h"
#include "uds_params.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

static UDSClientError_t sendHardReset(UDSClient_t *client, UDSSequence_t *seq) {
    uint8_t resetType = kHardReset;
    printf("%s: sending ECU reset type: %d\n", __func__, resetType);
    UDSSendECUReset(client, resetType);
    return kUDS_SEQ_ADVANCE;
}

static UDSClientError_t awaitPositiveResponse(UDSClient_t *client, UDSSequence_t *seq) {
    if (client->err) {
        return client->err;
    }
    if (kRequestStateIdle == client->state) {
        printf("got positive response\n");
        return kUDS_SEQ_ADVANCE;
    } else {
        return kUDS_SEQ_RUNNING;
    }
}

static UDSClientError_t awaitResponse(UDSClient_t *client, UDSSequence_t *seq) {
    if (kRequestStateIdle == client->state) {
        printf("got response\n");
        return kUDS_SEQ_ADVANCE;
    } else {
        return kUDS_SEQ_RUNNING;
    }
}

static UDSClientError_t requestSomeData(UDSClient_t *client, UDSSequence_t *seq) {
    printf("%s: calling ReadDataByIdentifier\n", __func__);
    static const uint16_t didList[] = {0x0001, 0x0008};
    UDSSendRDBI(client, didList, 2);
    return kUDS_SEQ_ADVANCE;
}

static UDSClientError_t printTheData(UDSClient_t *client, UDSSequence_t *seq) {
    uint8_t buf[21];
    uint16_t offset = 0;
    int err;

    printf("Unpacked:\n");
    err = UDSUnpackRDBIResponse(client, 0x0001, buf, DID_0x0001_LEN, &offset);
    if (err) {
        return err;
    }
    printf("DID 0x%04x: %d\n", 0x0001, buf[0]);

    err = UDSUnpackRDBIResponse(client, 0x0008, buf, DID_0x0008_LEN, &offset);
    if (err) {
        return err;
    }
    printf("DID 0x%04x: %s\n", 0x0008, buf);
    return kUDS_SEQ_ADVANCE;
}

static UDSClientError_t enterDiagnosticSession(UDSClient_t *client, UDSSequence_t *seq) {
    const uint8_t session = kExtendedDiagnostic;
    UDSSendDiagSessCtrl(client, session);
    printf("%s: entering diagnostic session %d\n", __func__, session);
    return kUDS_SEQ_ADVANCE;
}

static UDSClientError_t terminateServerProcess(UDSClient_t *client, UDSSequence_t *seq) {
    printf("%s: requesting server shutdown...\n", __func__);
    client->options |= SUPPRESS_POS_RESP;
    UDSSendRoutineCtrl(client, kStartRoutine, RID_TERMINATE_PROCESS, NULL, 0);
    return kUDS_SEQ_ADVANCE;
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

    terminateServerProcess,
    NULL,
};
// clang-format on

int main(int ac, char **av) {
    const char *ifname = "can0";
    if (ac >= 2) {
        ifname = av[1];
    }
    UDSClient_t client;

    UDSClientConfig_t cfg = {
        .if_name = ifname,
        .phys_recv_id = 0x7E8,
        .phys_send_id = 0x7E0,
        .func_send_id = 0x7DF,
    };

    if (UDSClientInit(&client, &cfg)) {
        exit(-1);
    }

    UDSSequence_t sequence;
    UDSSequenceInit(&sequence, callbacks, NULL);

    printf("running sequence. . .\n");
    int ret = 0;
    do {
        ret = UDSSequencePoll(&client, &sequence);
        SleepMillis(1);
    } while (ret > 0);
    printf("sequence completed with status: %d\n", ret);
    if (ret) {
        printf("client state: %d\n", client.state);
        printf("client err: %d\n", client.err);
    }

    UDSClientDeInit(&client);
    return ret;
}
