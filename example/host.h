#ifndef HOST_H
#define HOST_H

#include <stdint.h>
#include <stdbool.h>

extern bool g_should_exit;
int hostCANRxPoll(uint32_t *arb_id, uint8_t *data, uint8_t *size);
int hostSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size);
int hostmsleep();
uint32_t hostGetms();
int hostCANSetup();
void hostSetup(int ac, char **av);
void hostPrintf(const char *fmt, ...);

#endif
