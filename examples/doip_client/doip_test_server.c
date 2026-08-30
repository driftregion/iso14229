/**
 * @file doip_example.c
 * @brief Example usage of DoIP transport layer with UDS
 *
 * This example demonstrates:
 * 1. DoIP server receiving diagnostic requests
 * 2. DoIP client sending diagnostic requests
 * 3. Integration with UDS protocol (ISO 14229)
 *
 * To compile server:
 *   gcc -o doip_server_example doip_example.c doip_server.c -DSERVER_MODE
 *
 * To compile client:
 *   gcc -o doip_client_example doip_example.c doip_client.c -DCLIENT_MODE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "doip_server.h"

static volatile int server_running = 1;

void signal_handler(int signum) {
    (void)signum;  /* Unused parameter */
    printf("\nShutdown signal received\n");
    server_running = 0;
}


/**
 * @brief Callback for received diagnostic messages
 */
void on_diagnostic_message(uint16_t source_addr, const uint8_t *data, size_t len) {
    printf("\n=== Received UDS Request ===\n");
    printf("Source Address: 0x%04X\n", source_addr);
    printf("UDS Data (%zu bytes): ", len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    /* Example: Handle ReadDataByIdentifier (0x22) */
    if (len >= 3 && data[0] == 0x22) {
        uint16_t did = (data[1] << 8) | data[2];
        printf("Service: ReadDataByIdentifier (DID=0x%04X)\n", did);

        /* Build positive response */
        uint8_t response[10];
        response[0] = 0x62;  /* Positive response (0x22 + 0x40) */
        response[1] = data[1];
        response[2] = data[2];

        /* Example data for DID */
        response[3] = 0x12;
        response[4] = 0x34;
        response[5] = 0x56;
        response[6] = 0x78;

        /* Send response */
        doip_server_send_diag_response(source_addr, response, 7);
        printf("Sent positive response\n");
    } else
    /* Example: Handle WriteDataByIdentifier (0x2E) */
    if (len >= 4 && data[0] == 0x2E) {
        uint16_t did = (data[1] << 8) | data[2];
        printf("Service: WriteDataByIdentifier (DID=0x%04X)\n", did);

        /* Build positive response */
        uint8_t response[10];
        response[0] = 0x6E;  /* Positive response (0x2E + 0x40) */
        response[1] = data[1];
        response[2] = data[2];

        /* Send response */
        doip_server_send_diag_response(source_addr, response, 7);
        printf("Sent positive response\n");
    }

    /* Example: Handle DiagnosticSessionControl (0x10) */
    else if (len >= 2 && data[0] == 0x10) {
        uint8_t session_type = data[1];
        printf("Service: DiagnosticSessionControl (Session=0x%02X)\n", session_type);

        /* Build positive response */
        uint8_t response[6];
        response[0] = 0x50;  /* Positive response */
        response[1] = session_type;
        response[2] = 0x00;  /* P2 high byte */
        response[3] = 0x32;  /* P2 low byte (50ms) */
        response[4] = 0x01;  /* P2* high byte */
        response[5] = 0xF4;  /* P2* low byte (500ms) */

        doip_server_send_diag_response(source_addr, response, 6);
        printf("Sent positive response\n");
    }
    /* Example: Handle TesterPresent (0x3E) */
    else if (len >= 2 && data[0] == 0x3E) {
        printf("Service: TesterPresent\n");

        uint8_t response[2];
        response[0] = 0x7E;  /* Positive response */
        response[1] = data[1];

        doip_server_send_diag_response(source_addr, response, 2);
        printf("Sent positive response\n");
    }
    else {
        /* Unknown service - send negative response */
        uint8_t response[3];
        response[0] = 0x7F;  /* Negative response */
        response[1] = data[0];
        response[2] = 0x11;  /* ServiceNotSupported */

        doip_server_send_diag_response(source_addr, response, 3);
        printf("Sent negative response (ServiceNotSupported)\n");
    }

    printf("===========================\n\n");
}

int main(void) {
    printf("DoIP Server Example\n");
    printf("===================\n\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize DoIP server */
    uint16_t logical_address = 0x0001;
    if (doip_server_init(logical_address, on_diagnostic_message) < 0) {
        fprintf(stderr, "Failed to initialize DoIP server\n");
        return 1;
    }

    printf("Server running. Press Ctrl+C to stop.\n\n");

    /* Main loop */
    while (server_running) {
        doip_server_process(100);  /* 100ms timeout */
    }

    doip_server_shutdown();

    printf("Server stopped.\n");
    return 0;
}
