#ifndef TEST_ISO14229_H
#define TEST_ISO14229_H

#include <assert.h>
#include <stdint.h>
#include "iso14229.h"
#include "iso14229client.h"
#include "iso14229server.h"
#include "isotp-c/isotp.h"
#include "isotp-c/isotp_config.h"
#include "isotp-c/isotp_defines.h"

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"

#define ASSERT_INT_EQUAL(a, b)                                                                     \
    {                                                                                              \
        int _a = a;                                                                                \
        int _b = b;                                                                                \
        if ((_a) != (_b)) {                                                                        \
            printf("%s:%d (%d != %d)\n", __FILE__, __LINE__, _a, _b);                              \
            fflush(stdout);                                                                        \
            assert(0);                                                                             \
        }                                                                                          \
    }

#define ASSERT_PTR_EQUAL(a, b)                                                                     \
    {                                                                                              \
        void *_a = a;                                                                              \
        void *_b = b;                                                                              \
        if ((_a) != (_b)) {                                                                        \
            printf("%s:%d (%p != %p)\n", __FILE__, __LINE__, _a, _b);                              \
            fflush(stdout);                                                                        \
            assert(0);                                                                             \
        }                                                                                          \
    }

#define ASSERT_MEMORY_EQUAL(a, b, len)                                                             \
    {                                                                                              \
        const uint8_t *_a = a;                                                                     \
        const uint8_t *_b = b;                                                                     \
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

#define SERVER_SEND_ID (0x1U)      /* server sends */
#define SERVER_PHYS_RECV_ID (0x2U) /* server listens for physically (1:1) addressed messages */
#define SERVER_FUNC_RECV_ID (0x4U) /* server listens for functionally (1:n) addressed messages */

#define CLIENT_PHYS_SEND_ID SERVER_PHYS_RECV_ID /* client sends physically (1:1) by default */
#define CLIENT_SEND_ID CLIENT_PHYS_SEND_ID
#define CLIENT_FUNC_SEND_ID SERVER_FUNC_RECV_ID
#define CLIENT_RECV_ID SERVER_SEND_ID

#define CLIENT_DEFAULT_P2_MS 150
#define CLIENT_DEFAULT_P2_STAR_MS 1500
#define SERVER_DEFAULT_P2_MS 50
#define SERVER_DEFAULT_P2_STAR_MS 2000
#define SERVER_DEFAULT_S3_MS 5000

// TODO: parameterize and fuzz this
#define DEFAULT_ISOTP_BUFSIZE (2048U)

struct IsoTpLinkConfig {
    uint16_t send_id;
    uint8_t *send_buffer;
    uint16_t send_buf_size;
    uint8_t *recv_buffer;
    uint16_t recv_buf_size;
    uint32_t (*user_get_ms)(void);
    int (*user_send_can)(uint32_t arbitration_id, const uint8_t *data, uint8_t size);
    void (*user_debug)(const char *message, ...);
};

static inline void IsoTpInitLink(IsoTpLink *link, const struct IsoTpLinkConfig *cfg) {
    isotp_init_link(link, cfg->send_id, cfg->send_buffer, cfg->send_buf_size, cfg->recv_buffer,
                    cfg->recv_buf_size, cfg->user_get_ms, cfg->user_send_can, cfg->user_debug);
}

#define DEFAULT_SERVER_CONFIG()                                                                    \
    {                                                                                              \
        .phys_recv_id = SERVER_PHYS_RECV_ID, .func_recv_id = SERVER_FUNC_RECV_ID,                  \
        .send_id = SERVER_SEND_ID, .phys_link = &g.srvPhysLink, .func_link = &g.srvFuncLink,       \
        .phys_link_receive_buffer = g.srvPhysLinkRxBuf,                                            \
        .phys_link_recv_buf_size = sizeof(g.srvPhysLinkRxBuf),                                     \
        .phys_link_send_buffer = g.srvPhysLinkTxBuf,                                               \
        .phys_link_send_buf_size = sizeof(g.srvPhysLinkTxBuf),                                     \
        .func_link_receive_buffer = g.srvFuncLinkRxBuf,                                            \
        .func_link_recv_buf_size = sizeof(g.srvFuncLinkRxBuf),                                     \
        .func_link_send_buffer = g.srvFuncLinkTxBuf,                                               \
        .func_link_send_buf_size = sizeof(g.srvFuncLinkTxBuf),                                     \
        .userSessionTimeoutCallback = mockSessionTimeoutHandler, .userGetms = mockUserGetms,       \
        .userCANTransmit = mockServerCANTransmit, .userCANRxPoll = mockServerCANRxPoll,            \
        .p2_ms = SERVER_DEFAULT_P2_MS, .p2_star_ms = SERVER_DEFAULT_P2_STAR_MS,                    \
        .s3_ms = SERVER_DEFAULT_S3_MS                                                              \
    }

#define DEFAULT_CLIENT_CONFIG()                                                                    \
    {                                                                                              \
        .phys_send_id = CLIENT_PHYS_SEND_ID,                                                       \
        .func_send_id = CLIENT_FUNC_SEND_ID,                                                       \
        .recv_id = CLIENT_RECV_ID,                                                                 \
        .p2_ms = CLIENT_DEFAULT_P2_MS,                                                             \
        .p2_star_ms = CLIENT_DEFAULT_P2_STAR_MS,                                                   \
        .link = &g.clientLink,                                                                     \
        .link_receive_buffer = g.clientLinkRxBuf,                                                  \
        .link_recv_buf_size = sizeof(g.clientLinkRxBuf),                                           \
        .link_send_buffer = g.clientLinkTxBuf,                                                     \
        .link_send_buf_size = sizeof(g.clientLinkTxBuf),                                           \
        .userCANTransmit = mockClientSendCAN,                                                      \
        .userGetms = mockUserGetms,                                                                \
        .userCANRxPoll = mockClientCANRxPoll,                                                      \
        .userDebug = mockClientLinkDbg,                                                            \
    };

#define TEST_SETUP()                                                                               \
    memset(&g, 0, sizeof(g));                                                                      \
    printf("%s\n", __PRETTY_FUNCTION__);

#define TEST_TEARDOWN() printf(ANSI_BOLD "OK\n" ANSI_RESET);

struct CANMessage {
    uint32_t arbId;
    uint8_t data[8];
    uint8_t size;
};

#endif
