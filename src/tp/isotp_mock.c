#if defined(UDS_TP_ISOTP_MOCK)

/// \cond INTERNAL_INTERFACE

#include "tp/isotp_mock.h"
#include "iso14229.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NUM_TP 16u
#define NUM_MSGS 8u
static ISOTPMock_t TPs[MAX_NUM_TP];
static const char ZeroTestBlock[sizeof(ISOTPMock_t)];
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
    unsigned i = 0;
    while (i < MsgCount) {
        if (UDSTimeAfter(UDSMillis(), msgs[i].scheduled_tx_time)) {
            bool found = false;
            for (unsigned j = 0; j < MAX_NUM_TP; j++) {
                ISOTPMock_t *tp = &TPs[j];
                if (0 == memcmp(tp, ZeroTestBlock, sizeof(ISOTPMock_t))) {
                    continue; // slot not in use
                }
                if ((tp->sa_phys == msgs[i].info.A_TA) || (tp->sa_func == msgs[i].info.A_TA)) {
                    found = true;
                    if (tp->recv_len > 0u) {
                        UDS_LOGW(__FILE__,
                                 "TPMock: %s recv buffer is already full. Message dropped",
                                 tp->name);
                        continue;
                    }

                    UDS_LOGD(__FILE__,
                             "%s receives %ld bytes from TA=0x%03X (A_TA_Type=%s):", tp->name,
                             msgs[i].len, msgs[i].info.A_TA,
                             (msgs[i].info.A_TA_Type == UDS_A_TA_TYPE_PHYSICAL) ? "PHYSICAL"
                                                                                : "FUNCTIONAL");
                    UDS_LOG_SDU(__FILE__, msgs[i].buf, msgs[i].len, &(msgs[i].info));

                    (void)memmove(tp->recv_buf, msgs[i].buf, msgs[i].len);
                    tp->recv_len = msgs[i].len;
                    tp->recv_info = msgs[i].info;
                }
            }

            if (!found) {
                UDS_LOGW(__FILE__, "TPMock: no matching receiver for message");
            }

            for (unsigned j = i + 1u; j < MsgCount; j++) {
                msgs[j - 1u] = msgs[j];
            }
            MsgCount--;
        } else {
            i++;
        }
    }
}

static UDSErr_t mock_tp_send(struct UDSTp *hdl, const uint8_t *buf, size_t len,
                             const UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl; // cppcheck-suppress [misra-c2012-11.3]
    if (MsgCount >= NUM_MSGS) {
        UDS_LOGW(__FILE__, "mock_tp_send: too many messages in the queue");
        return UDS_FAIL;
    }
    struct Msg *m = &msgs[MsgCount];
    MsgCount++;
    UDS_A_TA_Type_t ta_type =
        (info == NULL) ? (UDS_A_TA_Type_t)UDS_A_TA_TYPE_PHYSICAL : (UDS_A_TA_Type_t)info->A_TA_Type;
    m->len = len;
    m->info.A_AE = (info == NULL) ? 0u : info->A_AE;
    if (UDS_A_TA_TYPE_PHYSICAL == ta_type) {
        m->info.A_TA = tp->ta_phys;
        m->info.A_SA = tp->sa_phys;
    } else if (UDS_A_TA_TYPE_FUNCTIONAL == ta_type) {

        // This condition is only true for standard CAN.
        // Technically CAN-FD may also be used in ISO-TP.
        // TODO: add profiles to isotp_mock
        if (len > 7u) {
            UDS_LOGW(__FILE__, "mock_tp_send: functional message too long: %zu", len);
            return UDS_FAIL;
        }
        m->info.A_TA = tp->ta_func;
        m->info.A_SA = tp->sa_func;
    } else {
        UDS_LOGW(__FILE__, "mock_tp_send: unknown TA type: %d", ta_type);
        return UDS_FAIL;
    }
    m->info.A_TA_Type = ta_type;
    m->scheduled_tx_time = UDSMillis() + tp->send_tx_delay_ms;
    (void)memmove(m->buf, buf, len);

    UDS_LOGD(__FILE__, "%s sends %ld bytes to TA=0x%03X (A_TA_Type=%s):", tp->name, len,
             m->info.A_TA,
             (m->info.A_TA_Type == UDS_A_TA_TYPE_PHYSICAL) ? "PHYSICAL" : "FUNCTIONAL");
    UDS_LOG_SDU(__FILE__, buf, len, &m->info);

    return UDS_OK;
}

static UDSErr_t mock_tp_recv(struct UDSTp *hdl, uint8_t *buf, size_t bufsiz, size_t *recvlen,
                             UDSSDU_t *info) {
    UDS_ASSERT(hdl);
    ISOTPMock_t *tp = (ISOTPMock_t *)hdl; // cppcheck-suppress [misra-c2012-11.3]
    if (tp->recv_len == 0u) {
        return UDS_OK;
    }
    if (bufsiz < tp->recv_len) {
        UDS_LOGE(__FILE__, "mock_tp_recv: buffer too small: %ld < %ld", bufsiz, tp->recv_len);
        return UDS_FAIL;
    }
    *recvlen = tp->recv_len;
    memmove(buf, tp->recv_buf, tp->recv_len);
    if (info != NULL) {
        *info = tp->recv_info;
    }
    tp->recv_len = 0;
    return UDS_OK;
}

static UDSErr_t mock_tp_poll(struct UDSTp *hdl) {
    (void)hdl; // unused parameter
    NetworkPoll();
    return UDS_OK;
}

static void ISOTPMockAttach(ISOTPMock_t *tp, const ISOTPMockArgs_t *args) {
    UDS_ASSERT(tp);
    UDS_ASSERT(args);
    UDS_ASSERT(TPCount < MAX_NUM_TP);
    TPCount++;
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
    UDS_ASSERT(tp);
    for (unsigned i = 0; i < MAX_NUM_TP; i++) {
        if (&TPs[i] == tp) {
            TPCount--;
            UDS_LOGV(__FILE__, "TPMock: detached %s. TPCount: %d", tp->name, TPCount);
            return;
        }
    }
    UDS_ASSERT(false);
}

UDSTp_t *ISOTPMockNew(const char *name, const ISOTPMockArgs_t *args) {
    if (TPCount >= MAX_NUM_TP) {
        UDS_LOGI(__FILE__, "TPCount: %d, too many TPs\n", TPCount);
        return NULL;
    }
    size_t i = 0;
    do {
        if (0 == memcmp(&TPs[i], ZeroTestBlock, sizeof(ISOTPMock_t))) {
            break;
        }
        i++;
    } while (i < MAX_NUM_TP);
    UDS_ASSERT(i != MAX_NUM_TP);
    ISOTPMock_t *tp = &TPs[i];
    UDS_ASSERT(tp);
    (void)memset(tp, 0, sizeof(ISOTPMock_t));
    if (name != NULL) {
        if (snprintf(tp->name, sizeof(tp->name), "%s", name) >= (int)sizeof(tp->name)) {
            UDS_LOGE(__FILE__, "Transport name too long, truncated");
        }
    } else {
        (void)snprintf(tp->name, sizeof(tp->name), "TPMock%u", TPCount);
    }
    ISOTPMockAttach(tp, args);
    return &tp->hdl;
}

void ISOTPMockConnect(UDSTp_t *tp1, UDSTp_t *tp2);

void ISOTPMockLogToFile(const char *filename) {
    if (LogFile != NULL) {
        (void)fprintf(stderr, "Log file is already open\n");
        return;
    }
    if (!filename) {
        (void)fprintf(stderr, "Filename is NULL\n");
        return;
    }
    // create file
    LogFile = fopen(filename, "w");
    if (NULL == LogFile) {
        (void)fprintf(stderr, "Failed to open log file %s\n", filename);
        return;
    }
}

void ISOTPMockLogToStdout(void) {
    if (NULL == LogFile) {
        return;
    }
    LogFile = stdout;
}

void ISOTPMockReset(void) {
    (void)memset(TPs, 0, sizeof(TPs));
    TPCount = 0;
    (void)memset(msgs, 0, sizeof(msgs));
    MsgCount = 0;
}

void ISOTPMockFree(UDSTp_t *tp) {
    UDS_ASSERT(tp);
    UDS_ASSERT(0 != memcmp(tp, ZeroTestBlock, sizeof(ISOTPMock_t)));
    ISOTPMock_t *tpm = (ISOTPMock_t *)tp; // cppcheck-suppress [misra-c2012-11.3]
    ISOTPMockDetach(tpm);
    (void)memset(tp, 0, sizeof(ISOTPMock_t));
}

/// \endcond INTERNAL_INTERFACE

#endif
