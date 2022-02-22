#ifndef SIMPLE_H
#define SIMPLE_H

#include "../iso14229.h"
#include <stdint.h>

/**
 * @brief
 *
 * @param arb_id
 * @param data
 * @param size
 * @return int 0 if message, -1 if no message, -2 if error
 */
extern int hostCANRxPoll(uint32_t *arb_id, uint8_t *data, uint8_t *size);

void simpleServerInit();
void simpleServerPeriodicTask();

#endif
