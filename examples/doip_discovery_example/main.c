#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "tp/doip/doip_client.h"

static bool select_by_vin(const DoIPDiscoveryInfo *info, void *user) {
    const char *needle = (const char *)user; /* expected VIN prefix or full VIN */
    if (!needle || !*needle) return false;
    if (info->vin[0] == '\0') return false;
    return strncmp(info->vin, needle, strlen(needle)) == 0;
}

int main(int argc, char **argv) {
    const char *vin_prefix = argc > 1 ? argv[1] : NULL;
    bool loopback = argc > 2 ? (strcmp(argv[2], "loopback") == 0) : false;

    DoIPClient_t tp;
    memset(&tp, 0, sizeof(tp));

    /* Set a selection callback (optional): choose server whose VIN matches prefix */
    UDSDoIPSetSelectionCallback(&tp, select_by_vin, (void*)vin_prefix);

    /* Discover vehicles (multicast by default; pass "loopback" to use 127.0.0.1) */
    int count = UDSDoIPDiscoverVehicles(&tp, 2000, loopback);
    printf("Discovered %d responders\n", count);

    if (tp.server_ip[0] == '\0') {
        printf("No server selected. Exiting.\n");
        return 0;
    }

    /* Demonstration ends at discovery/selection. Connection can be done via UDSDoIPInitClient. */
    printf("Selected server: %s\n", tp.server_ip);
    return 0;
}
