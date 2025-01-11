/**
 * @file isotp_mock.h
 * @brief in-memory ISO15765 (ISO-TP) transport layer implementation for testing
 * @date 2023-10-21
 *
 */
#if defined(UDS_TP_ISOTP_MOCK)

#pragma once

#include "iso14229.h"

typedef struct ISOTPMock {
    UDSTpHandle_t hdl;
    uint8_t recv_buf[UDS_TP_MTU];
    uint8_t send_buf[UDS_TP_MTU];
    size_t recv_len;
    UDSSDU_t recv_info;
    uint32_t sa_phys;          // source address - physical messages are sent from this address
    uint32_t ta_phys;          // target address - physical messages are sent to this address
    uint32_t sa_func;          // source address - functional messages are sent from this address
    uint32_t ta_func;          // target address - functional messages are sent to this address
    uint32_t send_tx_delay_ms; // simulated delay
    uint32_t send_buf_size;    // simulated size of the send buffer
    char name[32];             // name for logging
} ISOTPMock_t;

typedef struct {
    uint32_t sa_phys; // source address - physical messages are sent from this address
    uint32_t ta_phys; // target address - physical messages are sent to this address
    uint32_t sa_func; // source address - functional messages are sent from this address
    uint32_t ta_func; // target address - functional messages are sent to this address
} ISOTPMockArgs_t;

#define ISOTPMock_DEFAULT_CLIENT_ARGS                                                              \
    &(ISOTPMockArgs_t) {                                                                           \
        .sa_phys = 0x7E8, .ta_phys = 0x7E0, .sa_func = UDS_TP_NOOP_ADDR, .ta_func = 0x7DF          \
    }
#define ISOTPMock_DEFAULT_SERVER_ARGS                                                              \
    &(ISOTPMockArgs_t) {                                                                           \
        .sa_phys = 0x7E0, .ta_phys = 0x7E8, .sa_func = 0x7DF, .ta_func = UDS_TP_NOOP_ADDR          \
    }

/**
 * @brief Create a mock transport. It is connected by default to a broadcast network of all other
 * mock transports in the same process.
 * @param name optional name of the transport (can be NULL)
 * @return UDSTpHandle_t*
 */
UDSTpHandle_t *ISOTPMockNew(const char *name, ISOTPMockArgs_t *args);
void ISOTPMockFree(UDSTpHandle_t *tp);

/**
 * @brief write all messages to a file
 * @note uses UDSMillis() to get the current time
 * @param filename log file name (will be overwritten)
 */
void ISOTPMockLogToFile(const char *filename);
void ISOTPMockLogToStdout(void);

/**
 * @brief clear all transports and close the log file
 */
void ISOTPMockReset(void);

#endif
