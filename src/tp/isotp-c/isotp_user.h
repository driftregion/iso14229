#ifndef ISOTPC_USER_H
#define ISOTPC_USER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief user implemented, print debug message */
void isotp_user_debug(const char* message, ...);

/**
 * @brief user implemented, send can message. should return ISOTP_RET_OK when success.
 * 
 * @return may return ISOTP_RET_NOSPACE if the CAN transfer should be retried later
 * or ISOTP_RET_ERROR if transmission couldn't be completed
 */
int  isotp_user_send_can(const uint32_t arbitration_id,
                         const uint8_t* data, const uint8_t size
#if ISO_TP_USER_SEND_CAN_ARG
,void *arg
#endif                         
                         );

/**
 * @brief user implemented, gets the amount of time passed since the last call in microseconds
 */
uint32_t isotp_user_get_us(void);

#ifdef __cplusplus
}
#endif

#endif // ISOTPC_USER_H

