#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mbedtls/config.h>
#include <mbedtls/platform.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>

typedef struct {
    enum {
        Step_0_RequestSeed,
        Step_1_ReceiveSeed,
        Step_2_SendKey,
        Step_3_ReceiveKeyResponse,
        Step_DONE,
    } step;
    UDSErr_t err;
    uint8_t seed[256];
    size_t seed_len;
} SequenceContext_t;

static int sign(const uint8_t *seed, size_t seed_len, uint8_t *key, size_t key_len) {
    int ret = 0;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    const char *pers = "rsa_sign";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers,
                          strlen(pers));

    mbedtls_pk_init(&pk);

    const char *private_key_pem = "private_key.pem";

    if (mbedtls_pk_parse_keyfile(&pk, private_key_pem, NULL) != 0) {
        mbedtls_printf("Failed to parse private key\n");
        ret = -1;
        goto exit;
    }

    // Verify that the loaded key is an RSA key
    if (mbedtls_pk_get_type(&pk) != MBEDTLS_PK_RSA) {
        mbedtls_printf("Loaded key is not an RSA key\n");
        ret = -1;
        goto exit;
    }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);

    // Allocate buffer for the signature
    size_t sig_len = mbedtls_rsa_get_len(rsa);
    if (sig_len != key_len) {
        fprintf(stderr, "sig_len: %zu != %zu\n", sig_len, key_len);
        ret = -1;
        goto exit;
    }

    // Perform RSA signing operation
    if (mbedtls_rsa_rsassa_pkcs1_v15_sign(rsa, mbedtls_ctr_drbg_random, &ctr_drbg,
                                          MBEDTLS_RSA_PRIVATE, MBEDTLS_MD_SHA256, seed_len, seed,
                                          key) != 0) {
        mbedtls_printf("Failed to sign data\n");
        ret = -1;
        goto exit;
    }

    // 'key' now contains the RSA signature

exit:
    mbedtls_rsa_free(rsa);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return ret;
}

UDSErr_t fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    SequenceContext_t *c = (SequenceContext_t *)client->fn_data;
    if (evt != UDS_EVT_Poll) {
        UDS_LOGI(__FILE__, "%s (%d)", UDSEventToStr(evt), evt);
    }
    if (UDS_EVT_Err == evt) {
        UDS_LOGE(__FILE__, "Exiting on step %d with error: %s", c->step,
                 UDSErrToStr(*(UDSErr_t *)ev_data));
        c->err = *(UDSErr_t *)ev_data;
        c->step = Step_DONE;
    }
    switch (c->step) {
    case Step_0_RequestSeed: {
        c->err = UDSSendSecurityAccess(client, 3, NULL, 0);
        if (c->err) {
            UDS_LOGE(__FILE__, "UDSSendSecurityAccess failed with err: %s", UDSErrToStr(c->err));
            c->step = Step_DONE;
        }
        c->step = Step_1_ReceiveSeed;
        break;
    }
    case Step_1_ReceiveSeed: {
        if (UDS_EVT_ResponseReceived == evt) {
            struct SecurityAccessResponse sar = {0};
            c->err = UDSUnpackSecurityAccessResponse(client, &sar);
            if (c->err) {
                UDS_LOGE(__FILE__, "UDSUnpackSecurityAccessResponse failed with err: %s",
                         UDSErrToStr(c->err));
                c->step = Step_DONE;
                break;
            }

            printf("seed: ");
            for (int i = 0; i < sar.securitySeedLength; i++) {
                printf("%02X ", sar.securitySeed[i]);
            }
            printf("\n");

            // Check if all bytes in the seed are 0
            bool all_zero = true;
            for (int i = 0; i < sar.securitySeedLength; i++) {
                if (sar.securitySeed[i] != 0) {
                    all_zero = false;
                    break;
                }
            }

            if (all_zero) {
                UDS_LOGI(__FILE__, "seed is all zero, already unlocked");
                c->step = Step_DONE;
                break;
            }

            // Store seed for later use
            memcpy(c->seed, sar.securitySeed, sar.securitySeedLength);
            c->seed_len = sar.securitySeedLength;
            c->step = Step_2_SendKey;
        }
        break;
    }
    case Step_2_SendKey: {
        uint8_t key[512] = {0};
        if (sign(c->seed, c->seed_len, key, sizeof(key))) {
            UDS_LOGE(__FILE__, "sign failed");
            c->err = UDS_FAIL;
            c->step = Step_DONE;
            break;
        }

        c->err = UDSSendSecurityAccess(client, 4, key, sizeof(key));
        if (c->err) {
            UDS_LOGE(__FILE__, "UDSSendSecurityAccess failed with err: %s", UDSErrToStr(c->err));
            c->step = Step_DONE;
            break;
        }
        c->step = Step_3_ReceiveKeyResponse;
        break;
    }
    case Step_3_ReceiveKeyResponse: {
        if (UDS_EVT_ResponseReceived == evt) {
            UDS_LOGI(__FILE__, "Security access unlocked");
            c->step = Step_DONE;
        }
        break;
    }

    default:
        break;
    }
    return UDS_OK;
}

int main(int ac, char **av) {
    UDSClient_t client;
#if defined(UDS_TP_ISOTP_SOCK)
    UDSTpIsoTpSock_t tp;
    if (UDSTpIsoTpSockInitClient(&tp, "vcan0", 0x7E8, 0x7E0, 0x7DF)) {
        UDS_LOGE(__FILE__, "UDSTpIsoTpSockInitClient failed");
        exit(-1);
    }
#elif defined(UDS_TP_ISOTP_C_SOCKETCAN)
    UDSTpISOTpC_t tp;
    if (UDSTpISOTpCInit((UDSTpISOTpC_t *)&tp, "vcan0", 0x7E8, 0x7E0, 0x7DF, 0x7FF)) {
        UDS_LOGE(__FILE__, "UDSTpISOTpCInit failed");
        exit(-1);
    }
#else
#error "no transport defined"
#endif

    if (UDSClientInit(&client)) {
        exit(-1);
    }

    client.tp = (UDSTp_t *)&tp;
    client.fn = fn;

    SequenceContext_t ctx = {0};
    client.fn_data = &ctx;

    UDS_LOGI(__FILE__, "polling");
    while (ctx.step != Step_DONE) {
        UDSClientPoll(&client);
    }

    return ctx.err;
}
