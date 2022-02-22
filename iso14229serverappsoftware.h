#ifndef APPSOFTWARE_H
#define APPSOFTWARE_H

#include <stdbool.h>
#include <stdint.h>
#include "iso14229server.h"

int udsAppInit(void *self, const void *cfg, Iso14229Server *iso14229);

#define ISO14229_MIDDLEWARE_APPSOFTWARE()                                                          \
    { .self = NULL, .cfg = NULL, .initFunc = udsAppInit, .pollFunc = NULL }

#endif
