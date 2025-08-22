#if defined(UDS_TP_ISOTP_MOCK)

#include "tp/isotp_mock.h"
#include "iso14229.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NUM_TP 16
#define NUM_MSGS 8
static ISOTPMock_t *TPs[MAX_NUM_TP];
static unsigned TPCount = 0;
static FILE *LogFile = NULL;
static struct Msg {
    uint8_t buf[UDS_ISOTP_MTU];
    size_t len;
    UDSSDU_t info;
    uint32_t scheduled_tx_time;
    ISOTPMock_t *sender;
} msgs[NUM_MSGS];
static unsigned MsgCount = 0;

static void NetworkPoll(void) {
    for (unsigned i = 0; i < MsgCount; i++) {
        if (UDSTimeAfter(UDSMillis(), msgs[i].scheduled_tx_time)) {
            bool found = false;
            for (unsigned j = 0; j < TPCount; j++) {
                ISOTPMock_t *tp = TPs[j];
                if (tp->sa_phys == msgs[i].info.A_TA || tp->sa_func == msgs[i].info.A_TA) {
                    found = true;
                    if (tp->recv_len > 0) {
                        UDS_LOGW(__FILE__,
                                 "TPMock: %s recv buffer is already full. Message dropped",
                                 tp->name);
                        continue;
                    }

                    UDS_LOGD(__FILE__,
                             "%s receives %ld bytes from TA=0x%03X (A_TA_Type=%s):", tp->name,
                             msgs[i].len, msgs[i].info.A_TA,
                             msgs[i].info.A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "PHYSICAL"
                                                                              : "FUNCTIONAL");
                    UDS_LOG_SDU(__FILE__, msgs[i].buf, msgs[i].len, &(msgs[i].info));

                    memmove(tp->recv_buf, msgs[i].buf, msgs[i].len);
                    tp->recv_len = msgs[i].len;
                    tp->recv_info = msgs[i].info;
                }
            }

            if (!found) {
                UDS_LOGW(__FILE__, "TPMock: no matching receiver for message");
            }

            for (unsigned j = i + 1; j < MsgCount; j++) {
                msgs[j - 1] = msgs[j];
            }
            MsgCount--;
            i--;
        }
    }
}

static ssize_t mock_tp_send(struct UDSTp *hdl, uint8_t *buf, size_t len, UDSSDU_t *info) {
    assert(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl;
    if (MsgCount >= NUM_MSGS) {
        UDS_LOGW(__FILE__, "mock_tp_send: too many messages in the queue");
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

        // This condition is only true for standard CAN.
        // Technically CAN-FD may also be used in ISO-TP.
        // TODO: add profiles to isotp_mock
        if (len > 7) {
            UDS_LOGW(__FILE__, "mock_tp_send: functional message too long: %ld", len);
            return -1;
        }
        m->info.A_TA = tp->ta_func;
        m->info.A_SA = tp->sa_func;
    } else {
        UDS_LOGW(__FILE__, "mock_tp_send: unknown TA type: %d", ta_type);
        return -1;
    }
    m->info.A_TA_Type = ta_type;
    m->scheduled_tx_time = UDSMillis() + tp->send_tx_delay_ms;
    memmove(m->buf, buf, len);

    UDS_LOGD(__FILE__, "%s sends %ld bytes to TA=0x%03X (A_TA_Type=%s):", tp->name, len,
             m->info.A_TA, m->info.A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ? "PHYSICAL" : "FUNCTIONAL");
    UDS_LOG_SDU(__FILE__, buf, len, &m->info);

    return len;
}

static ssize_t mock_tp_recv(struct UDSTp *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info) {
    assert(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl;
    if (tp->recv_len == 0) {
        return 0;
    }
    if (bufsize < tp->recv_len) {
        UDS_LOGW(__FILE__, "mock_tp_recv: buffer too small: %ld < %ld", bufsize, tp->recv_len);
        return -1;
    }
    int len = tp->recv_len;
    memmove(buf, tp->recv_buf, tp->recv_len);
    if (info) {
        *info = tp->recv_info;
    }
    tp->recv_len = 0;
    return len;
}

static UDSTpStatus_t mock_tp_poll(struct UDSTp *hdl) {
    NetworkPoll();
    // todo: make this status reflect TX time
    return UDS_TP_IDLE;
}

static_assert(offsetof(ISOTPMock_t, hdl) == 0, "ISOTPMock_t must not have any members before hdl");

static void ISOTPMockAttach(ISOTPMock_t *tp, ISOTPMockArgs_t *args) {
    assert(tp);
    assert(args);
    assert(TPCount < MAX_NUM_TP);
    TPs[TPCount++] = tp;
    tp->hdl.send = mock_tp_send;
    tp->hdl.recv = mock_tp_recv;
    tp->hdl.poll = mock_tp_poll;
    tp->sa_func = args->sa_func;
    tp->sa_phys = args->sa_phys;
    tp->ta_func = args->ta_func;
    tp->ta_phys = args->ta_phys;
    tp->recv_len = 0;
    UDS_LOGV(__FILE__, "attached %s. TPCount: %d", tp->name, TPCount);
}

static void ISOTPMockDetach(ISOTPMock_t *tp) {
    assert(tp);
    for (unsigned i = 0; i < TPCount; i++) {
        if (TPs[i] == tp) {
            for (unsigned j = i + 1; j < TPCount; j++) {
                TPs[j - 1] = TPs[j];
            }
            TPCount--;
            UDS_LOGV(__FILE__, "TPMock: detached %s. TPCount: %d", tp->name, TPCount);
            return;
        }
    }
    assert(false);
}

UDSTp_t *ISOTPMockNew(const char *name, ISOTPMockArgs_t *args) {
    if (TPCount >= MAX_NUM_TP) {
        UDS_LOGI(__FILE__, "TPCount: %d, too many TPs\n", TPCount);
        return NULL;
    }
    ISOTPMock_t *tp = malloc(sizeof(ISOTPMock_t));
    memset(tp, 0, sizeof(ISOTPMock_t));
    if (name) {
        strncpy(tp->name, name, sizeof(tp->name));
    } else {
        snprintf(tp->name, sizeof(tp->name), "TPMock%u", TPCount);
    }
    ISOTPMockAttach(tp, args);
    return &tp->hdl;
}

void ISOTPMockConnect(UDSTp_t *tp1, UDSTp_t *tp2);

void ISOTPMockLogToFile(const char *filename) {
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

void ISOTPMockLogToStdout(void) {
    if (LogFile) {
        return;
    }
    LogFile = stdout;
}

void ISOTPMockReset(void) {
    memset(TPs, 0, sizeof(TPs));
    TPCount = 0;
    memset(msgs, 0, sizeof(msgs));
    MsgCount = 0;
}

void ISOTPMockFree(UDSTp_t *tp) {
    ISOTPMock_t *tpm = (ISOTPMock_t *)tp;
    ISOTPMockDetach(tpm);
    free(tp);
}

#endif
