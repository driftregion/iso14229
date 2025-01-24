#ifndef ENV_H
#define ENV_H

#include "iso14229.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>  // needed for cmocka
#include <cmocka.h>

// A mock server explicitly for client testing
typedef struct {
    UDSTp_t *tp;
    void *impl;
} MockServer_t;

struct Behavior {
    uint8_t req_data[UDS_TP_MTU];
    size_t req_len;
    uint8_t resp_data[UDS_TP_MTU];
    size_t resp_len;
    uint32_t delay_ms;
};

void MockServerPoll(MockServer_t *srv);
void MockServerAddBehavior(MockServer_t *srv, struct Behavior *b);
MockServer_t *MockServerNew(void);
void MockServerFree(MockServer_t *srv);

// Test environment
typedef struct {

    // the environment polls these objects in EnvRunMillis
    UDSServer_t *server;
    UDSClient_t *client;
    MockServer_t *mock_server;
    UDSTp_t *server_tp;
    UDSTp_t *client_tp;
    bool is_real_time; // if true, EnvRunMillis will run for a wall-time duration rather than
                       // simulated time. This makes tests much slower, so use it only when
                       // necessary.
    bool do_not_poll; // if true, EnvRunMillis will not poll any objects
} Env_t;

void EnvRunMillis(Env_t *env, uint32_t millis);

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
#define TEST_INT_GREATER(a, b) _TEST_INT_COND(a, b, >)
#define TEST_INT_EQUAL(a, b) _TEST_INT_COND(a, b, ==)
#define TEST_INT_NE(a, b) _TEST_INT_COND(a, b, !=)

#define TEST_ERR_EQUAL(a, b) \
{ \
        int _a = a;                                                                                \
        int _b = b;                                                                                \
        if (!((_a) == (_b))) {                                                                     \
            printf("%s:%d, %d (%s) != %d (%s)\n", __FILE__, __LINE__, _a, UDSErrToStr(_a), _b, UDSErrToStr(_b));                       \
            fflush(stdout);                                                                        \
            assert_int_equal(_a, _b); \
        }                                                                                          \
} 

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
#define EXPECT_WITHIN_MS(env, cond, timeout_ms)                                                         \
    {                                                                                              \
        uint32_t deadline = UDSMillis() + timeout_ms + 1;                                          \
        while (!(cond)) {                                                                          \
            EnvRunMillis(env, 1);                                                                      \
            TEST_INT_LE(UDSMillis(), deadline);                                                    \
        }                                                                                          \
    }

// Expect that a condition is true for a duration
#define EXPECT_WHILE_MS(env, cond, duration)                                                            \
    {                                                                                              \
        uint32_t deadline = UDSMillis() + duration;                                                \
        while (UDSTimeAfter(deadline, UDSMillis())) {                                              \
            assert_true(cond);                                                                     \
            EnvRunMillis(env, 1);                                                                      \
        }                                                                                          \
    }

// Expect that a condition
// - is false until 90% of a duration has passed,
// - and true before 110% of the duration has passed
#define EXPECT_IN_APPROX_MS(env, cond, duration)                                                        \
    {                                                                                              \
        const float tolerance = 0.1f;                                                              \
        uint32_t pre_deadline = UDSMillis() + (int)((duration) * (1.0f - tolerance));              \
        uint32_t post_deadline = UDSMillis() + (int)((duration) * (1.0f + tolerance));             \
        while (UDSTimeAfter(pre_deadline, UDSMillis())) {                                          \
            assert_true(!(cond));                                                                  \
            EnvRunMillis(env, 1);                                                                      \
        }                                                                                          \
        while (!(cond)) {                                                                          \
            TEST_INT_LE(UDSMillis(), post_deadline);                                               \
            EnvRunMillis(env, 1);                                                                      \
        }                                                                                          \
    }

#endif
