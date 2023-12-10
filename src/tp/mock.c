#if defined(UDS_TP_MOCK)

#include "tp/mock.h"
#include "iso14229.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NUM_TP 16
#define NUM_MSGS 8
static TPMock_t *TPs[MAX_NUM_TP];
static unsigned TPCount = 0;
static FILE *LogFile = NULL;
static struct Msg {
    uint8_t buf[UDS_ISOTP_MTU];
    size_t len;
    UDSSDU_t info;
    uint32_t scheduled_tx_time;
} msgs[NUM_MSGS];
static unsigned MsgCount = 0;

static void LogMsg(const char *prefix, const uint8_t *buf, size_t len, UDSSDU_t *info) {
    if (!LogFile) {
        return;
    }
    fprintf(LogFile, "%06d, %s sends, 0x%03x (%s), ", UDSMillis(), prefix, info->A_TA,
            info->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "phys" : "func");
    for (unsigned i = 0; i < len; i++) {
        fprintf(LogFile, "%02x ", buf[i]);
    }
    fprintf(LogFile, "\n");
    fflush(LogFile); // flush every time in case of crash
}

static void NetworkPoll() {
    for (unsigned i = 0; i < MsgCount; i++) {
        struct Msg *msg = &msgs[i];
        if (UDSTimeAfter(UDSMillis(), msg->scheduled_tx_time)) {
            for (unsigned j = 0; j < TPCount; j++) {
                TPMock_t *tp = TPs[j];
                if (tp->sa_phys == msg->info.A_TA || tp->sa_func == msg->info.A_TA) {
                    if (tp->recv_len > 0) {
                        fprintf(stderr, "TPMock: %s recv buffer is already full. Message dropped\n",
                                tp->name);
                        continue;
                    }
                    memmove(tp->recv_buf, msg->buf, msg->len);
                    tp->recv_len = msg->len;
                    tp->recv_info = msg->info;
                }
            }
            LogMsg("network", msg->buf, msg->len, &msg->info);
            for (unsigned j = i + 1; j < MsgCount; j++) {
                msgs[j - 1] = msgs[j];
            }
            MsgCount--;
            i--;
        }
    }
}

static ssize_t mock_tp_peek(struct UDSTpHandle *hdl, uint8_t **p_buf, UDSSDU_t *info) {
    assert(hdl);
    assert(p_buf);
    TPMock_t *tp = (TPMock_t *)hdl;
    if (p_buf) {
        *p_buf = tp->recv_buf;
    }
    if (info) {
        *info = tp->recv_info;
    }
    return tp->recv_len;
}

static ssize_t mock_tp_send(struct UDSTpHandle *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    assert(hdl);
    TPMock_t *tp = (TPMock_t *)hdl;
    if (MsgCount > NUM_MSGS) {
        fprintf(stderr, "TPMock: too many messages in the queue\n");
        return -1;
    }
    struct Msg *m = &msgs[MsgCount++];
    UDSTpAddr_t ta_type = info == NULL ? UDS_A_TA_TYPE_PHYSICAL : info->A_TA_Type;
    m->len = len;
    m->info.A_AE = info == NULL ? 0 : info->A_AE;
    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        m->info.A_TA = tp->ta_phys;
        m->info.A_SA = tp->sa_phys;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {
        m->info.A_TA = tp->ta_func;
        m->info.A_SA = tp->sa_func;
    } else {
        fprintf(stderr, "TPMock: unknown TA type: %d\n", ta_type);
        return -1;
    }
    m->info.A_TA_Type = ta_type;
    m->scheduled_tx_time = UDSMillis() + tp->send_tx_delay_ms;
    memmove(m->buf, buf, len);
    LogMsg(tp->name, buf, len, &m->info);
    return len;
}

static UDSTpStatus_t mock_tp_poll(struct UDSTpHandle *hdl) {
    NetworkPoll();
    // todo: make this status reflect TX time
    return UDS_TP_IDLE;
}

static ssize_t mock_tp_get_send_buf(struct UDSTpHandle *hdl, uint8_t **p_buf) {
    assert(hdl);
    assert(p_buf);
    TPMock_t *tp = (TPMock_t *)hdl;
    *p_buf = tp->send_buf;
    return sizeof(tp->send_buf);
}

static void mock_tp_ack_recv(struct UDSTpHandle *hdl) {
    assert(hdl);
    TPMock_t *tp = (TPMock_t *)hdl;
    tp->recv_len = 0;
}

static_assert(offsetof(TPMock_t, hdl) == 0, "TPMock_t must not have any members before hdl");

static void TPMockAttach(TPMock_t *tp, TPMockArgs_t *args) {
    assert(tp);
    assert(args);
    assert(TPCount < MAX_NUM_TP);
    TPs[TPCount++] = tp;
    tp->hdl.peek = mock_tp_peek;
    tp->hdl.send = mock_tp_send;
    tp->hdl.poll = mock_tp_poll;
    tp->hdl.get_send_buf = mock_tp_get_send_buf;
    tp->hdl.ack_recv = mock_tp_ack_recv;
    tp->sa_func = args->sa_func;
    tp->sa_phys = args->sa_phys;
    tp->ta_func = args->ta_func;
    tp->ta_phys = args->ta_phys;
    tp->recv_len = 0;
}

static void TPMockDetach(TPMock_t *tp) {
    assert(tp);
    for (unsigned i = 0; i < TPCount; i++) {
        if (TPs[i] == tp) {
            for (unsigned j = i + 1; j < TPCount; j++) {
                TPs[j - 1] = TPs[j];
            }
            TPCount--;
            printf("TPMock: detached %s. TPCount: %d\n", tp->name, TPCount);
            return;
        }
    }
    assert(false);
}

UDSTpHandle_t *TPMockNew(const char *name, TPMockArgs_t *args) {
    if (TPCount >= MAX_NUM_TP) {
        printf("TPCount: %d, too many TPs\n", TPCount);
        return NULL;
    }
    TPMock_t *tp = malloc(sizeof(TPMock_t));
    if (name) {
        strncpy(tp->name, name, sizeof(tp->name));
    } else {
        snprintf(tp->name, sizeof(tp->name), "TPMock%d", TPCount);
    }
    TPMockAttach(tp, args);
    return &tp->hdl;
}

void TPMockConnect(UDSTpHandle_t *tp1, UDSTpHandle_t *tp2);

void TPMockLogToFile(const char *filename) {
    if (LogFile) {
        fprintf(stderr, "Log file is already open\n");
        return;
    }
    if (!filename) {
        fprintf(stderr, "Filename is NULL\n");
        return;
    }
    // create file
    LogFile = fopen(filename, "w");
    if (!LogFile) {
        fprintf(stderr, "Failed to open log file %s\n", filename);
        return;
    }
}

void TPMockLogToStdout(void) {
    if (LogFile) {
        return;
    }
    LogFile = stdout;
}

void TPMockReset(void) {
    memset(TPs, 0, sizeof(TPs));
    TPCount = 0;
}

void TPMockFree(UDSTpHandle_t *tp) {
    TPMock_t *tpm = (TPMock_t *)tp;
    TPMockDetach(tpm);
    free(tp);
}

#endif 
