#ifndef ENV_H
#define ENV_H

#include "iso14229.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>  // needed for cmocka
#include <cmocka.h>

enum ENV_TpType {
    ENV_TP_TYPE_MOCK,       // tp/mock.h
    ENV_TP_TYPE_ISOTP_SOCK, // tp/isotp_sock.h
    ENV_TP_TYPE_ISOTPC,     // tp/isotp_c_socketcan.h
};

/**
 * @brief Environment options
 */
typedef struct {
    enum ENV_TpType tp_type; // transport type
    const char *ifname;      // CAN interface name used for isotp_sock and socketcan
    uint32_t srv_src_addr, srv_target_addr, srv_src_addr_func;
    uint32_t cli_src_addr, cli_target_addr, cli_tgt_addr_func;
    bool assert_no_client_err;
} ENV_Opts_t;

void ENV_ServerInit(UDSServer_t *srv);
void ENV_ClientInit(UDSClient_t *client);

#define ENV_SERVER_INIT(srv)                                                                       \
    ENV_ServerInit(&srv);                                                                          \
    TPMockLogToStdout();

#define ENV_CLIENT_INIT(client)                                                                    \
    ENV_ClientInit(&client);                                                                       \
    TPMockLogToStdout();

/**
 * @brief return a transport configured as client
 * @return UDSTpHandle_t*
 */
UDSTpHandle_t *ENV_TpNew(const char *name);
void ENV_TpFree(UDSTpHandle_t *tp);
void ENV_RegisterServer(UDSServer_t *server);
void ENV_RegisterClient(UDSClient_t *client);
void ENV_RunMillis(uint32_t millis);
const ENV_Opts_t *ENV_GetOpts();
void ENV_AttachHook(void (*fn)(void *), void *arg);


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
