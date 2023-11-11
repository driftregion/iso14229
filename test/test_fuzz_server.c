#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "iso14229.h"

#ifdef __cplusplus
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#else
int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#endif

uint8_t retval = kPositiveResponse;

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) { return retval; }

struct Impl {
    UDSTpHandle_t hdl;
    uint8_t buf[8192];
    size_t size;
};

static ssize_t tp_recv(UDSTpHandle_t *hdl, void *buf, size_t count, UDSTpAddr_t *ta_type) {
    struct Impl *pL_Impl = (struct Impl *)hdl;
    if (pL_Impl->size < count) {
        count = pL_Impl->size;
    }
    memmove(buf, pL_Impl->buf, count);
    pL_Impl->size = 0;
    return count;
}

ssize_t tp_send(struct UDSTpHandle *hdl, const void *buf, size_t count, UDSTpAddr_t ta_type) {
    return count;
}

UDSTpStatus_t tp_poll(struct UDSTpHandle *hdl) { return 0; }

static struct Impl impl = {
    .hdl =
        {
            .recv = tp_recv,
            .send = tp_send,
            .poll = tp_poll,
        },
    .buf = {0},
    .size = 0,
};

static uint32_t g_ms = 0;
uint32_t UDSMillis() { return g_ms; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    UDSServer_t srv;
    UDSServerConfig_t cfg = {
        .fn = fn,
        .tp = &impl.hdl,
    };
    if (size < 1) {
        return 0;
    }

    retval = data[0];
    size = size - 1;

    if (size > sizeof(impl.buf)) {
        size = sizeof(impl.buf);
    }
    memmove(impl.buf, data, size);
    impl.size = size;
    UDSServerInit(&srv, &cfg);
    for (g_ms = 0; g_ms < 100; g_ms++) {
        UDSServerPoll(&srv);
    }
    return 0;
}
