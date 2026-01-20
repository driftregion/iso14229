#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "tp/doip/doip_client.h"

static bool select_by_vin(const DoIPDiscoveryInfo *info, void *user) {
    const char *needle = (const char *)user; /* expected VIN prefix or full VIN */
    if (!needle || !*needle) return false;
    if (info->vin[0] == '\0') return false;
    bool match = strncmp(info->vin, needle, strlen(needle)) == 0;
    printf("Discovered: VIN=%s IP=%s PORT=%u%s\n", info->vin, info->ip, info->remote_port,
           match ? " [MATCH]" : "");
    return match;
}

int main(int argc, char **argv) {
    const char *vin_prefix = NULL;
    bool loopback = false;
    uint16_t udp_port = 0; /* 0=>default */
    bool request_only = false;
    bool dump_raw = false;

    if (argc > 1) {
        if (strcmp(argv[1], "loopback") == 0) {
            loopback = true;
            if (argc > 2) {
                if (strcmp(argv[2], "--request-only") == 0) request_only = true;
                else if (strcmp(argv[2], "--raw") == 0) dump_raw = true;
                else udp_port = (uint16_t)atoi(argv[2]);
            }
            if (argc > 3) {
                if (strcmp(argv[3], "--request-only") == 0) request_only = true;
                else if (strcmp(argv[3], "--raw") == 0) dump_raw = true;
            }
        } else {
            vin_prefix = argv[1];
            if (argc > 2 && strcmp(argv[2], "loopback") == 0) {
                loopback = true;
                if (argc > 3) {
                    if (strcmp(argv[3], "--request-only") == 0) request_only = true;
                    else if (strcmp(argv[3], "--raw") == 0) dump_raw = true;
                    else udp_port = (uint16_t)atoi(argv[3]);
                }
                if (argc > 4) {
                    if (strcmp(argv[4], "--request-only") == 0) request_only = true;
                    else if (strcmp(argv[4], "--raw") == 0) dump_raw = true;
                }
            }
        }
    }

    DoIPClient_t tp;
    memset(&tp, 0, sizeof(tp));

    /* Set a selection callback (optional): choose server whose VIN matches prefix */
    if (vin_prefix && *vin_prefix) {
        UDSDoIPSetSelectionCallback(&tp, select_by_vin, (void*)vin_prefix);
    }

    /* Discover vehicles (multicast by default; pass "loopback" to use 127.0.0.1; optional port) */
    UDSDoIPSetDiscoveryOptions(request_only, dump_raw);
    int count = UDSDoIPDiscoverVehiclesEx(&tp, 2000, loopback, udp_port);
    printf("Discovered %d responders\n", count);

    if (tp.server_ip[0] == '\0') {
        printf("No server selected. Exiting.\n");
        return 0;
    }

    /* Demonstration ends at discovery/selection. Connection can be done via UDSDoIPInitClient. */
    printf("Selected server: %s\n", tp.server_ip);
    if (udp_port != 0) {
        printf("(Discovery UDP port override: %u)\n", udp_port);
    }
    return 0;
}
