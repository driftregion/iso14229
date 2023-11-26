#ifndef TEST_TEST_H
#define TEST_TEST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "iso14229.h"
#include "test/env.h"
#include "tp/mock.h"
#include <cmocka.h>

#define _TEST_INT_COND(a, b, cond)                                                                 \
    {                                                                                              \
        int _a = a;                                                                                \
        int _b = b;                                                                                \
        if (!((_a)cond(_b))) {                                                                     \
            printf("%s:%d (%d %s %d)\n", __FILE__, __LINE__, _a, #cond, _b);                       \
            fflush(stdout);                                                                        \
            assert_true(a cond b);                                                                 \
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
            assert_true(0);                                                                        \
        }                                                                                          \
    }

#define EXPECT_OK(stmt)                                                                            \
    {                                                                                              \
        int _ret = stmt;                                                                           \
        if (_ret != UDS_OK) {                                                                      \
            printf("%s:%d (%d != %d)\n", __FILE__, __LINE__, _ret, UDS_OK);                        \
            fflush(stdout);                                                                        \
            assert_true(_ret == UDS_OK);                                                           \
        }                                                                                          \
    }

// Expect that a condition is true within a timeout
#define EXPECT_WITHIN_MS(cond, timeout_ms)                                                         \
    {                                                                                              \
        uint32_t deadline = UDSMillis() + timeout_ms + 1;                                          \
        while (!(cond)) {                                                                          \
            TEST_INT_LE(UDSMillis(), deadline);                                                    \
            ENV_RunMillis(1);                                                                      \
        }                                                                                          \
    }

// Expect that a condition is true for a duration
#define EXPECT_WHILE_MS(cond, duration)                                                            \
    {                                                                                              \
        uint32_t deadline = UDSMillis() + duration;                                                \
        while (UDSTimeAfter(deadline, UDSMillis())) {                                              \
            assert_true(cond);                                                                     \
            ENV_RunMillis(1);                                                                      \
        }                                                                                          \
    }

// Expect that a condition
// - is false until 90% of a duration has passed,
// - and true before 110% of the duration has passed
#define EXPECT_IN_APPROX_MS(cond, duration)                                                        \
    {                                                                                              \
        const float tolerance = 0.1f;                                                              \
        uint32_t pre_deadline = UDSMillis() + (int)((duration) * (1.0f - tolerance));              \
        uint32_t post_deadline = UDSMillis() + (int)((duration) * (1.0f + tolerance));             \
        while (UDSTimeAfter(pre_deadline, UDSMillis())) {                                          \
            assert_true(!(cond));                                                                  \
            ENV_RunMillis(1);                                                                      \
        }                                                                                          \
        while (!(cond)) {                                                                          \
            TEST_INT_LE(UDSMillis(), post_deadline);                                               \
            ENV_RunMillis(1);                                                                      \
        }                                                                                          \
    }

#endif
