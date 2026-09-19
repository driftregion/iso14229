#ifndef ISOTP_EXAMPLE_PLATFORM_H
#define ISOTP_EXAMPLE_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ExampleCan ExampleCan;

bool example_can_receive(ExampleCan* can, uint32_t* arbitration_id, uint8_t* data, uint8_t* size);
int example_can_send(ExampleCan* can, uint32_t arbitration_id, const uint8_t* data, uint8_t size, bool fd, bool brs);
uint32_t example_get_us(void);
void example_process_message(const uint8_t* payload, uint32_t size);
void example_store_chunk(const uint8_t* payload, uint32_t size, bool complete);

#endif
