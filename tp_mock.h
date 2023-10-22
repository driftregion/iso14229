/**
 * @file tp_mock.h
 * @brief in-memory transport layer implementation for testing
 * @date 2023-10-21
 *
 */

#include "iso14229.h"

/**
 * @brief Create a mock transport. It is connected by default to a broadcast network of all other
 * mock transports in the same process.
 * @param name optional name of the transport (can be NULL)
 * @return UDSTpHandle_t*
 */
UDSTpHandle_t *TPMockCreate(const char *name);

/**
 * @brief write all messages to a file
 * @note uses UDSMillis() to get the current time
 * @param filename log file name (will be overwritten)
 */
void TPMockLogToFile(const char *filename);

/**
 * @brief clear all transports and close the log file
 */
void TPMockReset(void);
