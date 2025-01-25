#pragma once

#if UDS_SYS == UDS_SYS_WINDOWS

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#endif
