#pragma once

#if UDS_SYS == UDS_SYS_WINDOWS

#include <stdlib.h>
#include <time.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#endif
