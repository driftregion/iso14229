#include "iso14229.h"
#include "uds_aes_key.h"
#include "aes_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

static UDSServer_t srv;
static UDSTpIsoTpSock_t tp;
static bool done = false;
static uint8_t current_seed[16] = {0};      // Current 16-byte seed for 0x29 Authentication
static bool authentication_required = true; // Require authentication before RDBI
static bool is_authenticated = false;       // Track authentication status
static const uint8_t algorithm_indicator[16] = {0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
                                                0x04, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};

// Sample data for RDBI demonstration
static uint8_t sample_vin[18] = "1HGBH41JXMN109186"; // Sample VIN (Vehicle Identification Number)
static uint16_t sample_ecu_serial = 0x1234;          // Sample ECU serial number

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    switch (ev) {
    case UDS_EVT_DiagSessCtrl:
        printf("Diagnostic Session Control request received\n");
        return UDS_OK;
    case UDS_EVT_SessionTimeout:
        printf("Session timeout occurred\n");
        return UDS_OK;
    case UDS_EVT_AuthTimeout:
        is_authenticated = false;
        printf("Authentication timeout occurred\n");
        return UDS_OK;
    case UDS_EVT_Auth: {
        UDSAuthArgs_t *r = (UDSAuthArgs_t *)arg;

        printf("Authentication request received, subfunction: 0x%02X\n", r->type);

        switch (r->type) {
        case UDS_LEV_AT_RCFA: { // requestChallengeForAuthentication (0x05)
            printf("Request Challenge For Authentication\n");

            if (memcmp(r->subFuncArgs.reqChallengeArgs.algoInd, algorithm_indicator, 16) != 0) {
                printf("Unsupported algorithm indicator\n");
                return UDS_NRC_ConditionsNotCorrect;
            }

            // Generate a new random seed
            if (generate_random_seed(current_seed) != 0) {
                printf("Failed to generate random seed\n");
                return UDS_NRC_GeneralReject;
            }

            printf("Generated seed:\t\t\t");
            for (int i = 0; i < 16; i++) {
                printf("%02X ", current_seed[i]);
            }
            printf("\n");

            // Send response challenge
            uint8_t seed_len[] = {0x00, 0x10}; // 16 bytes
            r->copy(srv, seed_len, sizeof(seed_len));
            r->copy(srv, current_seed, sizeof(current_seed));

            uint8_t additional_param_len[] = {0x00, 0x00};
            r->copy(srv, additional_param_len, sizeof(additional_param_len));

            return r->set_auth_state(srv, UDS_AT_RA);
        }

        case UDS_LEV_AT_VPOWNU: { // verifyProofOfOwnershipUnidirectional (0x06)
            printf("Verify Proof Of Ownership Unidirectional\n");

            if (memcmp(r->subFuncArgs.verifyPownArgs.algoInd, algorithm_indicator, 16) != 0) {
                printf("Unsupported algorithm indicator\n");
                return UDS_NRC_ConditionsNotCorrect;
            }

            // Extract proof of ownership (encrypted seed)
            if (r->subFuncArgs.verifyPownArgs.pownLen != 16) {
                printf("Invalid proof of ownership length: %d (expected 16)\n",
                       r->subFuncArgs.verifyPownArgs.pownLen);
                return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
            }

            uint8_t *encrypted_proof = (uint8_t *)r->subFuncArgs.verifyPownArgs.pown;
            printf("Received encrypted proof:\t");
            for (int i = 0; i < 16; i++) {
                printf("%02X ", encrypted_proof[i]);
            }
            printf("\n");

            // Decrypt the proof using our AES key
            uint8_t decrypted_proof[16];
            if (aes_decrypt_ecb(uds_aes_key, encrypted_proof, decrypted_proof) != 0) {
                printf("Failed to decrypt proof of ownership\n");
                return UDS_NRC_GeneralReject;
            }

            printf("Decrypted proof:\t\t");
            for (int i = 0; i < 16; i++) {
                printf("%02X ", decrypted_proof[i]);
            }
            printf("\n");

            printf("Expected seed:\t\t\t");
            for (int i = 0; i < 16; i++) {
                printf("%02X ", current_seed[i]);
            }
            printf("\n");

            // Compare decrypted proof with the seed we sent
            if (memcmp(decrypted_proof, current_seed, 16) != 0) {
                printf("Proof verification failed - decrypted proof does not match seed\n");
                is_authenticated = false;
                return UDS_NRC_InvalidKey;
            }

            printf("Proof verification successful! Client is now authenticated.\n");
            is_authenticated = true;

            uint8_t session_key_len[] = {0x00, 0x00};
            r->copy(srv, session_key_len, sizeof(session_key_len));
            return r->set_auth_state(srv, UDS_AT_OVAC);
        }
        case UDS_LEV_AT_DA: { // deAuthenticate (0x00)
            printf("De-authenticate request\n");
            is_authenticated = false;

            uint8_t response_data[1] = {UDS_AT_DAS}; // DeAuthentication successful
            UDSErr_t copy_result = r->copy(srv, response_data, sizeof(response_data));
            if (copy_result != UDS_OK) {
                printf("Failed to copy de-authentication response\n");
                return copy_result;
            }

            return UDS_OK;
        }

        default:
            printf("Unsupported authentication subfunction: 0x%02X\n", r->type);
            return UDS_NRC_SubFunctionNotSupported;
        }
    }
    case UDS_EVT_ReadDataByIdent: {
        UDSRDBIArgs_t *r = (UDSRDBIArgs_t *)arg;
        printf("RDBI request for data identifier: 0x%04X\n", r->dataId);

        // Check if authentication is required and if client is authenticated
        if (authentication_required && !is_authenticated) {
            printf("Authentication required before RDBI access\n");
            return UDS_NRC_SecurityAccessDenied;
        }

        switch (r->dataId) {
        case 0xF190: // VIN (Vehicle Identification Number)
            printf("Responding with VIN data\n");
            return r->copy(srv, sample_vin, sizeof(sample_vin));

        case 0xF18C: // ECU Serial Number
            printf("Responding with ECU serial number\n");
            return r->copy(srv, &sample_ecu_serial, sizeof(sample_ecu_serial));

        default:
            printf("Unsupported data identifier: 0x%04X\n", r->dataId);
            return UDS_NRC_RequestOutOfRange;
        }
    }
    default:
        printf("Unhandled event: %d\n", ev);
        return UDS_NRC_ServiceNotSupported;
    }
}

static int sleep_ms(uint32_t tms) {
    struct timespec ts;
    int ret;
    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;
    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);
    return ret;
}

int main(int ac, char **av) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    if (UDSTpIsoTpSockInitServer(&tp, "vcan0", 0x7E0, 0x7E8, 0x7DF)) {
        fprintf(stderr, "UDSTpIsoTpSockInitServer failed\n");
        exit(-1);
    }

    if (UDSServerInit(&srv)) {
        fprintf(stderr, "UDSServerInit failed\n");
    }

    srv.tp = (UDSTp_t *)&tp;
    srv.fn = fn;

    printf("server up, polling . . .\n");
    while (!done) {
        UDSServerPoll(&srv);
        sleep_ms(1);
    }
    printf("server exiting\n");
    return 0;
}
