#ifndef PORT_H
#define PORT_H

#include <stdint.h>
#include <stdbool.h>

extern bool port_should_exit;
void portSetup(int ac, char **av);
extern void isotp_user_debug(const char *fmt, ...);

enum Iso14229CANRxStatus portCANRxPoll(uint32_t *arb_id, uint8_t *data, uint8_t *size);
int portSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size);
void portYieldms(long tms);
uint32_t portGetms();

#endif
