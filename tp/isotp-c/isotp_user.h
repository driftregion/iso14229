#ifndef __ISOTP_USER_H__
#define __ISOTP_USER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* user implemented, print debug message */
void isotp_user_debug(const char* message, ...);

/* user implemented, send can message. should return ISOTP_RET_OK when success.
*/
int  isotp_user_send_can(const uint32_t arbitration_id,
                         const uint8_t* data, const uint8_t size);

/* user implemented, get microsecond */
uint32_t isotp_user_get_us(void);

#ifdef __cplusplus
}
#endif

#endif // __ISOTP_H__

