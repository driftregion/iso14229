/**
 * @file tp_mock.h
 * @brief in-memory transport layer implementation for testing
 * @date 2023-10-21
 *
 */

#include "iso14229.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TPMock {
    UDSTpHandle_t hdl;
    uint8_t recv_buf[UDS_BUFSIZE];
    uint8_t send_buf[UDS_BUFSIZE];
    size_t recv_len;
    UDSSDU_t recv_info;
    uint32_t sa_phys; // source address - physical messages are sent from this address
    uint32_t ta_phys; // target address - physical messages are sent to this address
    uint32_t sa_func; // source address - functional messages are sent from this address
    uint32_t ta_func; // target address - functional messages are sent to this address
    uint32_t send_tx_delay_ms; // simulated delay
    uint32_t send_buf_size; // simulated size of the send buffer
    char name[32]; // name for logging
} TPMock_t;

typedef struct {
    uint32_t sa_phys; // source address - physical messages are sent from this address
    uint32_t ta_phys; // target address - physical messages are sent to this address
    uint32_t sa_func; // source address - functional messages are sent from this address
    uint32_t ta_func; // target address - functional messages are sent to this address
} TPMockArgs_t;

#define TPMOCK_DEFAULT_CLIENT_ARGS &(TPMockArgs_t){.sa_phys=0x7E8, .ta_phys=0x7E0, .sa_func=UDS_TP_NOOP_ADDR, .ta_func=0x7DF}
#define TPMOCK_DEFAULT_SERVER_ARGS &(TPMockArgs_t){.sa_phys=0x7E0, .ta_phys=0x7E8, .sa_func=0x7DF, .ta_func=UDS_TP_NOOP_ADDR}

/**
 * @brief Create a mock transport. It is connected by default to a broadcast network of all other
 * mock transports in the same process.
 * @param name optional name of the transport (can be NULL)
 * @return UDSTpHandle_t*
 */
UDSTpHandle_t *TPMockCreate(const char *name, TPMockArgs_t *args);

void TPMockAttach(TPMock_t *tp, TPMockArgs_t *args);
void TPMockDetach(TPMock_t *tp);

/**
 * @brief write all messages to a file
 * @note uses UDSMillis() to get the current time
 * @param filename log file name (will be overwritten)
 */
void TPMockLogToFile(const char *filename);
void TPMockLogToStdout(void);

/**
 * @brief clear all transports and close the log file
 */
void TPMockReset(void);

#ifdef __cplusplus
}
#endif

