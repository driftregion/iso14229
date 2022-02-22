#include "iso14229.h"
#include "iso14229server.h"
#include "iso14229serverappsoftware.h"

int udsAppInit(void *self, const void *cfg, Iso14229Server *iso14229) {
    iso14229ServerEnableService(iso14229, kSID_DIAGNOSTIC_SESSION_CONTROL);
    iso14229ServerEnableService(iso14229, kSID_ECU_RESET);
    iso14229ServerEnableService(iso14229, kSID_READ_DATA_BY_IDENTIFIER);
    iso14229ServerEnableService(iso14229, kSID_WRITE_DATA_BY_IDENTIFIER);
    iso14229ServerEnableService(iso14229, kSID_TESTER_PRESENT);
    return 0;
}