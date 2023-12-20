#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>

void BSPInit(void);
int BSPSetLED(uint8_t led, bool value);
int BSPSendCAN(uint32_t id, uint8_t *data, uint32_t len);

#endif
