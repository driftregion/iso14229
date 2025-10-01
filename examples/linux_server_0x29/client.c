#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>

// State machine for client operations
typedef enum {
    STATE_SEND_ECU_SERIAL_REQUEST,
    STATE_WAIT_ECU_SERIAL_RESPONSE,
    STATE_SEND_VIN_REQUEST,
    STATE_WAIT_VIN_RESPONSE,
    STATE_DONE
} ClientState_t;

typedef struct {
    ClientState_t state;
    char vin_data[18];   // VIN data (17 chars + null terminator)
    uint16_t ecu_serial; // ECU serial number
    bool done;
} ClientContext_t;

static int SleepMillis(uint32_t tms) {
    struct timespec ts;
    int ret;
    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;
    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);
    return ret;
}

static UDSErr_t ClientEventHandler(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    ClientContext_t *ctx = (ClientContext_t *)client->fn_data;

    if (evt == UDS_EVT_Err) {
        printf("Error occurred: %s\n", UDSErrToStr(*(UDSErr_t *)ev_data));
        ctx->done = true;
        return UDS_OK;
    }

    switch (ctx->state) {

    case STATE_SEND_ECU_SERIAL_REQUEST: {
        printf("Sending ECU Serial Number (0xF18C) request...\n");
        const uint16_t dids[] = {0xF18C};
        UDSErr_t err = UDSSendRDBI(client, dids, 1);
        if (err) {
            printf("Failed to send ECU serial request: %s\n", UDSErrToStr(err));
            ctx->done = true;
            return err;
        }
        ctx->state = STATE_WAIT_ECU_SERIAL_RESPONSE;
        break;
    }

    case STATE_WAIT_ECU_SERIAL_RESPONSE: {
        if (evt == UDS_EVT_ResponseReceived) {
            printf("ECU Serial response received, unpacking...\n");
            UDSRDBIVar_t vars[] = {
                {0xF18C, sizeof(ctx->ecu_serial), &ctx->ecu_serial, memmove},
            };

            UDSErr_t err = UDSUnpackRDBIResponse(client, vars, 1);
            if (err) {
                printf("Failed to unpack ECU serial response: %s\n", UDSErrToStr(err));
                ctx->done = true;
                return err;
            }

            printf("ECU Serial Number: 0x%04X (%d)\n", ctx->ecu_serial, ctx->ecu_serial);
            ctx->state = STATE_SEND_VIN_REQUEST;
        }
        break;
    }

    case STATE_SEND_VIN_REQUEST: {
        printf("Sending VIN (0xF190) request...\n");
        const uint16_t dids[] = {0xF190};
        UDSErr_t err = UDSSendRDBI(client, dids, 1);
        if (err) {
            printf("Failed to send VIN request: %s\n", UDSErrToStr(err));
            ctx->done = true;
            return err;
        }
        ctx->state = STATE_WAIT_VIN_RESPONSE;
        break;
    }

    case STATE_WAIT_VIN_RESPONSE: {
        if (evt == UDS_EVT_ResponseReceived) {
            printf("VIN response received, unpacking...\n");
            UDSRDBIVar_t vars[] = {
                {0xF190, sizeof(ctx->vin_data) - 1, ctx->vin_data, memmove},
            };

            UDSErr_t err = UDSUnpackRDBIResponse(client, vars, 1);
            if (err) {
                printf("Failed to unpack VIN response: %s\n", UDSErrToStr(err));
                ctx->done = true;
                return err;
            }

            ctx->vin_data[17] = '\0'; // Null terminate
            printf("VIN: %s\n", ctx->vin_data);
            ctx->state = STATE_DONE;
            ctx->done = true;
        }
        break;
    }

    case STATE_DONE:
        ctx->done = true;
        break;
    }

    return UDS_OK;
}

int main(int ac, char **av) {
    UDSClient_t client;
    UDSTpIsoTpSock_t tp;
    ClientContext_t ctx = {0};

    printf("UDS Client - RDBI Demo\n");
    printf("======================\n");

    if (UDSTpIsoTpSockInitClient(&tp, "vcan0", 0x7E8, 0x7E0, 0x7DF)) {
        fprintf(stderr, "UDSTpIsoTpSockInitClient failed\n");
        exit(-1);
    }

    if (UDSClientInit(&client)) {
        fprintf(stderr, "UDSClientInit failed\n");
        exit(-1);
    }

    client.tp = (UDSTp_t *)&tp;
    client.fn = ClientEventHandler;
    client.fn_data = &ctx;

    // Initialize client context
    ctx.state = STATE_SEND_ECU_SERIAL_REQUEST;
    ctx.done = false;

    printf("Starting RDBI requests...\n");

    // Main loop
    while (!ctx.done) {
        UDSClientPoll(&client);
        SleepMillis(1); // Small delay to prevent busy waiting
    }

    printf("RDBI demo completed.\n");
    return 0;
}
