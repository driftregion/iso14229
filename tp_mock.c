#include "tp_mock.h"
#include "iso14229.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>


typedef struct TPMock {
    UDSTpHandle_t hdl;
    UDSSDU_t recv_q[1];
    unsigned recv_cnt;
    uint8_t recv_buf[1024];
    uint32_t recv_buf_size;
    char name[32];
} TPMock_t;

static TPMock_t TPs[8];
static int TPCount= 0;
static FILE *LogFile = NULL;

static bool RecvBufIsFull(TPMock_t *tp) {
    return tp->recv_cnt >= sizeof(tp->recv_q) / sizeof(tp->recv_q[0]);
}

static void LogMsg(const char *prefix, UDSSDU_t *msg) {
    if (!LogFile) {
        return;
    }
    fprintf(LogFile, "%06d, %s sends, 0x%03x (%s), ", UDSMillis(),  prefix, msg->A_TA, msg->A_TA_Type == UDS_A_TA_TYPE_PHYSICAL ?  "phys": "func");
    for (unsigned i = 0; i < msg->A_Length; i++) {
        fprintf(LogFile, "%02x ", msg->A_Data[i]);
    }
    fprintf(LogFile, "\n");
    fflush(LogFile); // flush every time in case of crash
}

static ssize_t tp_recv(struct UDSTpHandle *hdl, UDSSDU_t *msg) {
    assert(hdl);
    assert(msg);
    assert(msg->A_Data);
    TPMock_t *tp = (TPMock_t *)hdl;
    if (tp->recv_cnt == 0) {
        return 0;
    } else {
        UDSSDU_t *in_msg = &tp->recv_q[0];
        if (in_msg->A_Length > msg->A_DataBufSize) {
            fprintf(stderr, "TPMock: %s recv buffer is too small\n", tp->name);
            return -1;
        }
        msg->A_Length = in_msg->A_Length;
        msg->A_Mtype = in_msg->A_Mtype;
        msg->A_SA = in_msg->A_SA;
        msg->A_TA = in_msg->A_TA;
        msg->A_TA_Type = in_msg->A_TA_Type;
        memmove((void*)msg->A_Data, in_msg->A_Data, in_msg->A_Length);

        for (unsigned i = 1; i < tp->recv_cnt; i++) {
            tp->recv_q[i - 1] = tp->recv_q[i];
        }
        tp->recv_cnt--;
        return msg->A_Length;
    }
}   

static ssize_t tp_send(struct UDSTpHandle *hdl, UDSSDU_t *msg) {
    assert(hdl);
    assert(msg);
    TPMock_t *tp = (TPMock_t *)hdl;
    for (unsigned i = 0; i < TPCount; i++) {
        TPMock_t *tp2 = &TPs[i];
        if (tp2 == tp) {
            continue; // don't send to self
        }
        if (RecvBufIsFull(tp2)) {
            fprintf(stderr, "TPMock: %s recv buffer is full\n", tp2->name);
            continue;
        }
        tp2->recv_q[tp2->recv_cnt++] = *msg;
    }
    LogMsg(tp->name, msg);
    return msg->A_Length;
}

static UDSTpStatus_t tp_poll(struct UDSTpHandle *hdl) {
    // todo: mock artificially long TX time
    return  UDS_TP_IDLE;
}

static_assert(offsetof(TPMock_t, hdl) == 0, "TPMock_t must not have any members before hdl");

UDSTpHandle_t *TPMockCreate(const char *name) {
    TPMock_t *tp;
    if (TPCount >= sizeof(TPs) / sizeof(TPs[0])) {
        return NULL;
    }
    tp = &TPs[TPCount++];
    if (name) {
        strncpy(tp->name, name, sizeof(tp->name));
    } else {
        snprintf(tp->name, sizeof(tp->name), "TPMock%d", TPCount);
    }
    tp->hdl.recv = tp_recv;
    tp->hdl.send = tp_send;
    tp->hdl.poll = tp_poll;
    return &tp->hdl;
}

void TPMockConnect(UDSTpHandle_t *tp1, UDSTpHandle_t *tp2);

void TPMockLogToFile(const char *filename){
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

void TPMockReset() {
    memset(TPs, 0, sizeof(TPs));
    TPCount = 0;
    if (LogFile) {
        fclose(LogFile);
        LogFile = NULL;
    }
}