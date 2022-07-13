#define ISO14229USERDEBUG printf
#include "../iso14229client.h"
#include "shared.h"
#include "client.h"
#include "port.h"

#define ISOTP_BUFSIZE 256
static IsoTpLink link;

static uint8_t isotpRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpSendBuf[ISOTP_BUFSIZE];

static struct Iso14229ClientConfig cfg = {
    .func_send_id = CLIENT_FUNC_SEND_ID,
    .phys_send_id = CLIENT_PHYS_SEND_ID,
    .recv_id = CLIENT_RECV_ID,
    .p2_ms = CLIENT_DEFAULT_P2_MS,
    .p2_star_ms = CLIENT_DEFAULT_P2_STAR_MS,
    .yield_period_ms = ISO14229_CLIENT_DEFAULT_YIELD_PERIOD_MS,
    .link = &link,
    .link_receive_buffer = isotpRecvBuf,
    .link_recv_buf_size = sizeof(isotpRecvBuf),
    .link_send_buffer = isotpSendBuf,
    .link_send_buf_size = sizeof(isotpSendBuf),
    .userCANTransmit = portSendCAN,
    .userCANRxPoll = portCANRxPoll,
    .userGetms = portGetms,
    .userYieldms = portYieldms,
    .userDebug = isotp_user_debug,
};

static enum Iso14229ClientError sendHardReset(Iso14229Client *client, void *args) {
    (void)args;
    uint8_t resetType = kHardReset;
    printf("sending ECU reset type: %d\n", resetType);
    ECUReset(client, resetType);
    return kISO14229_CLIENT_CALLBACK_DONE;
}

static enum Iso14229ClientError awaitPositiveResponse(Iso14229Client *client, void *args) {
    (void)args;
    if (client->err) {
        return client->err;
    }
    if (kRequestStateIdle == client->state) {
        return kISO14229_CLIENT_CALLBACK_DONE;
    } else {
        return kISO14229_CLIENT_CALLBACK_PENDING;
    }
}

static enum Iso14229ClientError awaitResponse(Iso14229Client *client, void *args) {
    (void)args;
    if (kRequestStateIdle == client->state) {
        return kISO14229_CLIENT_CALLBACK_DONE;
    } else {
        return kISO14229_CLIENT_CALLBACK_PENDING;
    }
}

static enum Iso14229ClientError requestSomeData(Iso14229Client *client, void *args) {
    (void)args;
    static const uint16_t didList[] = {0x0001, 0x0008};
    ReadDataByIdentifier(client, didList, 2);
    return kISO14229_CLIENT_CALLBACK_DONE;
}

static enum Iso14229ClientError printTheData(Iso14229Client *client, void *args) {
    (void)args;
    printf("The raw RDBI response looks like this: \n");
    PRINTHEX(client->resp.buf, client->resp.len);

    uint8_t buf[21];
    uint16_t offset = 0;
    int err;

    err = RDBIReadDID(&client->resp, 0x0001, buf, DID_0x0001_LEN, &offset);
    if (err) {
        return err;
    }
    printf("0x%04x: %d\n", 0x0001, buf[0]);

    err = RDBIReadDID(&client->resp, 0x0008, buf, DID_0x0008_LEN, &offset);
    if (err) {
        return err;
    }
    printf("0x%04x: %s\n", 0x0008, buf);
    return kISO14229_CLIENT_CALLBACK_DONE;
}

static enum Iso14229ClientError enterDiagnosticSession(Iso14229Client *client, void *args) {
    (void)args;
    DiagnosticSessionControl(client, kExtendedDiagnostic);
    return kISO14229_CLIENT_CALLBACK_DONE;
}

static enum Iso14229ClientError sendExitReset(Iso14229Client *client, void *args) {
    (void)args;
    uint8_t resetType = RESET_TYPE_EXIT;
    printf("sending ECU reset type: %d\n", resetType);
    ECUReset(client, resetType);
    return kISO14229_CLIENT_CALLBACK_DONE;
}

//
// 流程定义
// Sequence Definition
//

// clang-format off
static Iso14229ClientCallback callbacks[] = {
    sendHardReset,
    awaitPositiveResponse,

    requestSomeData,
    awaitPositiveResponse,
    printTheData,

    enterDiagnosticSession, 
    awaitResponse,

    sendExitReset,
    awaitPositiveResponse,
};
// clang-format on

int run_client_blocking() {
    Iso14229Client client;
    iso14229ClientInit(&client, &cfg);

    struct Iso14229Sequence sequence = {
        .list = callbacks,
        .len = sizeof(callbacks) / sizeof(callbacks[0]),
    };
    struct Iso14229Runner runner = {0};

    printf("running sequence. . .\n");
    int err = iso14229SequenceRunBlocking(&sequence, &client, &runner);
    printf("sequence completed with status: %d\n", err);
    if (err) {
        printf("client state: %d\n", client.state);
        printf("client err: %d\n", client.err);
        printf("link receive status: %d\n", link.receive_status);
        printf("link send status: %d\n", link.send_status);
    }
    return err;
}
