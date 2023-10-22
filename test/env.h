#ifndef ENV_H   
#define ENV_H

#include "iso14229.h"
#include <stdint.h>

UDSTpHandle_t *ENV_TpNew();


void ENV_Send(UDSSDU_t *msg);

#define ENV_EXPECT_MSG_WITHIN_MILLIS(msg_ptr, millis) \
    do { \
        UDSSDU_t *msg = (msg_ptr); \
        ENV_ExpectBytesWithinMillis(ENV_TpNew(), msg->A_Data, msg->A_Length, millis); \
    } while (0)


#endif 