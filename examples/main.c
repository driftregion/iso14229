#include "port.h"
#include "server.h"
#include "client.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int main(int ac, char **av) {
    if (ac < 2) {
        fprintf(stderr, "usage: %s [server,client]\n", av[0]);
        exit(0);
    }
    portSetup(ac - 1, av + 1);

    if (0 == strcmp(av[1], "server")) {
        run_server_blocking();
    } else if (0 == strcmp(av[1], "client")) {
        exit(run_client_blocking());
    } else {
        fprintf(stderr, "usage: %s [server,client]\n", av[0]);
        exit(0);
    }
}
