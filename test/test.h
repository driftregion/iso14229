#ifndef TEST_TEST_H
#define TEST_TEST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "iso14229.h"
#include "test/env.h"
#include "tp/mock.h"

#define _TEST_INT_COND(a, b, cond)                                                                 \
    {                                                                                              \
        int _a = a;                                                                                \
        int _b = b;                                                                                \
        if (!((_a)cond(_b))) {                                                                     \
            printf("%s:%d (%d %s %d)\n", __FILE__, __LINE__, _a, #cond, _b);                       \
            fflush(stdout);                                                                        \
            assert(a cond b);                                                                      \
        }                                                                                          \
    }

#define TEST_INT_LT(a, b) _TEST_INT_COND(a, b, <)
#define TEST_INT_LE(a, b) _TEST_INT_COND(a, b, <=)
#define TEST_INT_GE(a, b) _TEST_INT_COND(a, b, >=)
#define TEST_INT_EQUAL(a, b) _TEST_INT_COND(a, b, ==)
#define TEST_INT_NE(a, b) _TEST_INT_COND(a, b, !=)

#define TEST_PTR_EQUAL(a, b)                                                                       \
    {                                                                                              \
        const void *_a = a;                                                                        \
        const void *_b = b;                                                                        \
        if ((_a) != (_b)) {                                                                        \
            printf("%s:%d (%p != %p)\n", __FILE__, __LINE__, _a, _b);                              \
            fflush(stdout);                                                                        \
            assert(a == b);                                                                        \
        }                                                                                          \
    }

#define TEST_MEMORY_EQUAL(a, b, len)                                                               \
    {                                                                                              \
        const uint8_t *_a = (const uint8_t *)a;                                                    \
        const uint8_t *_b = (const uint8_t *)b;                                                    \
        if (memcmp(_a, _b, len)) {                                                                 \
            printf("A:");                                                                          \
            for (unsigned int i = 0; i < len; i++) {                                               \
                printf("%02x,", _a[i]);                                                            \
            }                                                                                      \
            printf(" (%s)\nB:", #a);                                                               \
            for (unsigned int i = 0; i < len; i++) {                                               \
                printf("%02x,", _b[i]);                                                            \
            }                                                                                      \
            printf(" (%s)\n", #b);                                                                 \
            fflush(stdout);                                                                        \
            assert(0);                                                                             \
        }                                                                                          \
    }

// expect a server response within a timeout
#define EXPECT_RESPONSE_WITHIN_MILLIS(d1, timeout_ms)                                              \
    {                                                                                              \
        uint32_t deadline = UDSMillis() + timeout_ms + 1;                                          \
        uint8_t buf[UDS_BUFSIZE];                                                                  \
        UDSSDU_t msg = {                                                                           \
            .A_DataBufSize = sizeof(buf),                                                          \
            .A_Data = buf,                                                                         \
        };                                                                                         \
        while (0 == UDSTpRecv(mock_tp, &msg)) {                                                    \
            TEST_INT_LE(UDSMillis(), deadline);                                                    \
            ENV_RunMillis(1);                                                                      \
        }                                                                                          \
        TEST_INT_EQUAL(msg.A_Length, sizeof(d1));                                                  \
        TEST_MEMORY_EQUAL(msg.A_Data, d1, sizeof(d1));                                             \
    }

#define EXPECT_RECV_WITHIN_MILLIS(tp, msg, timeout_ms)                                             \
    {                                                                                              \
                                                                                                   \
        uint32_t deadline = UDSMillis() + timeout_ms + 1;                                          \
        while (0 == UDSTpRecv(tp, msg)) {                                                          \
            TEST_INT_LE(UDSMillis(), deadline);                                                    \
            ENV_RunMillis(1);                                                                      \
        }                                                                                          \
    }

#define EXPECT_OK(stmt)                                                                            \
    {                                                                                              \
        int _ret = stmt;                                                                           \
        if (_ret != UDS_OK) {                                                                      \
            printf("%s:%d (%d != %d)\n", __FILE__, __LINE__, _ret, UDS_OK);                        \
            fflush(stdout);                                                                        \
            assert(_ret == UDS_OK);                                                                \
        }                                                                                          \
    }

#define EXPECT_WITHIN_MS(cond, timeout_ms)                                                         \
    {                                                                                              \
        uint32_t deadline = UDSMillis() + timeout_ms + 1;                                          \
        while (!(cond)) {                                                                          \
            TEST_INT_LE(UDSMillis(), deadline);                                                    \
            ENV_RunMillis(1);                                                                      \
        }                                                                                          \
    }

#endif
