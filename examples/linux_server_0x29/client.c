#include "iso14229.h"
#include "uds_aes_key.h"
#include "aes_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>

// State machine for client operations with authentication
typedef enum {
    STATE_SEND_CHANGE_SESSION_REQUEST,
    STATE_WAIT_CHANGE_SESSION_RESPONSE,
    STATE_SEND_ECU_SERIAL_FAIL_REQUEST,  // Send ECU Serial Number (0xF18C) request; expect failure
    STATE_WAIT_ECU_SERIAL_FAIL_RESPONSE, // Wait for ECU serial failure response
    STATE_REQUEST_SEED,                  // Request authentication seed (0x27 0x29)
    STATE_WAIT_SEED_RESPONSE,            // Wait for seed response
    STATE_SEND_KEY,                      // Send encrypted key (0x27 0x30)
    STATE_WAIT_KEY_RESPONSE,             // Wait for key validation response
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
    uint8_t seed[16];    // Received seed from server
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

    switch (ctx->state) {

    case STATE_SEND_CHANGE_SESSION_REQUEST: {
        printf("Sending Diagnostic Session Control request (Programming Session)...\n");
        UDSErr_t err = UDSSendDiagSessCtrl(client, UDS_LEV_DS_PRGS);
        if (err) {
            printf("Failed to send session change request: %s\n", UDSErrToStr(err));
            ctx->done = true;
            return err;
        }
        ctx->state = STATE_WAIT_CHANGE_SESSION_RESPONSE;
        break;
    }

    case STATE_WAIT_CHANGE_SESSION_RESPONSE: {
        if (evt == UDS_EVT_ResponseReceived) {
            printf("Session change response received - Now in Programming Session\n");
            ctx->state = STATE_SEND_ECU_SERIAL_FAIL_REQUEST;
        }
        break;
    }

    case STATE_SEND_ECU_SERIAL_FAIL_REQUEST: {
        printf("Sending ECU Serial Number (0xF18C) request (expecting failure due to "
               "authentication)...\n");
        const uint16_t dids[] = {0xF18C};
        UDSErr_t err = UDSSendRDBI(client, dids, 1);
        if (err) {
            printf("Failed to send ECU serial request: %s\n", UDSErrToStr(err));
            ctx->done = true;
            return err;
        }
        ctx->state = STATE_WAIT_ECU_SERIAL_FAIL_RESPONSE;
        break;
    }

    case STATE_WAIT_ECU_SERIAL_FAIL_RESPONSE: {

        if (evt == UDS_EVT_Err) {
            printf("Expected Error occurred: %s\n", UDSErrToStr(*(UDSErr_t *)ev_data));
            ctx->state = STATE_REQUEST_SEED;
            return UDS_OK;
        }

        if (evt == UDS_EVT_ResponseReceived) {
            printf("Expected an error but received a positive response! Is the server "
                   "already/still in an authenticated state?\n");
            ctx->state = STATE_REQUEST_SEED;
        }
        break;
    }

    case STATE_REQUEST_SEED: {
        printf("Requesting authentication seed (0x27 0x29)...\n");

        const uint8_t request_data[3 + 16] = {
            kSID_AUTHENTICATION, // Service ID
            UDS_LEV_AT_RCFA,     // requestChallengeForAuthentication
            0x00,                // communication Configuration

            /* Algorithm Indicator for AES-128-CBC */
            0x06,
            0x09,
            0x60,
            0x86,
            0x48,
            0x01,
            0x65,
            0x03,
            0x04,
            0x01,
            0x02,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
        };

        UDSErr_t err = UDSSendBytes(client, request_data, sizeof(request_data));
        if (err) {
            printf("Failed to send seed request: %s\n", UDSErrToStr(err));
            ctx->done = true;
            return err;
        }
        ctx->state = STATE_WAIT_SEED_RESPONSE;
        break;
    }

    case STATE_WAIT_SEED_RESPONSE: {
        if (evt == UDS_EVT_ResponseReceived) {
            printf("Seed response received, extracting seed...\n");

            if (client->recv_size < 21) {
                printf("Response too short: %u bytes\n", client->recv_size);
                ctx->done = true;
                return UDS_ERR_RESP_TOO_SHORT;
            }

            uint16_t challenge_len =
                (uint16_t)((uint16_t)client->recv_buf[19] << 8 | (uint16_t)client->recv_buf[20]);

            if (challenge_len != 16) {
                printf("Invalid challenge length: %u (expected 16)\n", challenge_len);
                ctx->done = true;
                return UDS_ERR_MISUSE;
            }

            uint8_t *seed = &client->recv_buf[21];

            // Copy the seed
            memcpy(ctx->seed, seed, 16);

            printf("Received seed:\t\t");
            for (int i = 0; i < 16; i++) {
                printf("%02X ", ctx->seed[i]);
            }
            printf("\n");

            ctx->state = STATE_SEND_KEY;
        }
        break;
    }

    case STATE_SEND_KEY: {
        printf("Encrypting seed and sending key (0x27 0x30)...\n");

        // Encrypt the seed with our AES key
        uint8_t encrypted_key[16];
        if (aes_encrypt_ecb(uds_aes_key, ctx->seed, encrypted_key) != 0) {
            printf("Failed to encrypt seed\n");
            ctx->done = true;
            return UDS_ERR_INVALID_ARG;
        }

        printf("Sending encrypted key:\t");
        for (int i = 0; i < 16; i++) {
            printf("%02X ", encrypted_key[i]);
        }
        printf("\n");

        uint8_t request_data[2 + 16 + 2 + 16 + 2 + 2] = {
            kSID_AUTHENTICATION, // Service ID
            UDS_LEV_AT_VPOWNU,   // requestVerifyPown

            /* Algorithm Indicator for AES-128-CBC */
            0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02, 0x00, 0x00, 0x00,
            0x00, 0x00,

            0x00, 0x10, // Length of pown

            // We add the pown below
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00,

            0x00, 0x00, // length of challenge client
            0x00, 0x00, // length of additional parameters
        };

        memcpy(&request_data[20], encrypted_key, 16);

        UDSErr_t err = UDSSendBytes(client, request_data, sizeof(request_data));
        if (err) {
            printf("Failed to send key: %s\n", UDSErrToStr(err));
            ctx->done = true;
            return err;
        }
        ctx->state = STATE_WAIT_KEY_RESPONSE;
        break;
    }

    case STATE_WAIT_KEY_RESPONSE: {
        if (evt == UDS_EVT_ResponseReceived) {
            if (client->recv_size < 3 || client->recv_buf[2] != UDS_AT_OVAC) {
                printf("Key validation failed or invalid response\n");
                ctx->done = true;
                return UDS_ERR_MISUSE;
            }

            printf("Key validation response received - Authentication successful!\n");
            ctx->state = STATE_SEND_ECU_SERIAL_REQUEST;
        }
        break;
    }

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

    if (evt == UDS_EVT_Err) {
        printf("Error occurred: %s\n", UDSErrToStr(*(UDSErr_t *)ev_data));
        ctx->done = true;
        return UDS_OK;
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
    ctx.state = STATE_SEND_CHANGE_SESSION_REQUEST; // Start with session change
    ctx.done = false;

    printf("Starting session change, authentication and RDBI requests...\n");

    // Main loop
    while (!ctx.done) {
        UDSClientPoll(&client);
        SleepMillis(1); // Small delay to prevent busy waiting
    }

    printf("Session change, authentication and RDBI demo completed.\n");
    return 0;
}
