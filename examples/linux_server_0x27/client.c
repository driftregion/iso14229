#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <mbedtls/config.h>
#include <mbedtls/platform.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>


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

int sign(const uint8_t *seed, size_t seed_len, uint8_t* key, size_t key_len) {
    int ret = 0;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    const char *pers = "rsa_sign";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers));

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

    // // Allocate buffer for the signature
    size_t sig_len = mbedtls_rsa_get_len(rsa);
    if (sig_len != key_len) {
        fprintf(stderr, "sig_len: %ld != %ld\n", sig_len, sizeof(key));
        ret = -1;
        goto exit;
    }

    // Perform RSA signing operation
    if (mbedtls_rsa_rsassa_pkcs1_v15_sign(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE,
                                          MBEDTLS_MD_SHA256, seed_len, seed, key) != 0) {
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


int main(int ac, char **av) {
    UDSClient_t client;
    UDSTpIsoTpSock_t tp;
    UDSErr_t err = 0;

    if (UDSTpIsoTpSockInitClient(&tp, "vcan0", 0x7E8, 0x7E0, 0x7DF)) {
        fprintf(stderr, "UDSTpIsoTpSockInitClient failed\n");
        exit(-1);
    }

    if (UDSClientInit(&client)) {
        exit(-1);
    }

    client.tp = (UDSTp_t *)&tp;

    // Request seed
    err = UDSSendSecurityAccess(&client, 3, NULL, 0);
    if (err) {
        fprintf(stderr, "UDSSendSecurityAccess failed: %d\n", err);
        exit(-1);
    }

    while (UDSClientPoll(&client));

    if (client.err) {
        fprintf(stderr, "UDSSendSecurityAccess failed: %d\n", client.err);
        exit(-1);
    }

    struct SecurityAccessResponse sar = {0};
    err = UDSUnpackSecurityAccessResponse(&client, &sar);
    if (err) {
        fprintf(stderr, "UDSUnpackSecurityAccessResponse failed: %d\n", err);
        exit(-1);
    }

    printf("seed: ");
    for (int i = 0; i < sar.securitySeedLength; i++) {
        printf("%02X ", sar.securitySeed[i]);
    }
    
    // Check if all bytes in the seed are 0
    bool all_zero = true;
    for (int i = 0; i < sar.securitySeedLength; i++) {
        if (sar.securitySeed[i] != 0) {
            all_zero = false;
            break;
        }
    }

    if (all_zero) {
        fprintf(stderr, "seed is all zero, already unlocked\n");
        return 0;
    }


    uint8_t key[512] = {0};

    if (sign(sar.securitySeed, sar.securitySeedLength, key, sizeof(key))) {
        fprintf(stderr, "sign failed\n");
        exit(-1);
    }


    err = UDSSendSecurityAccess(&client, 4, key, sizeof(key));
    if (err) {
        fprintf(stderr, "UDSSendSecurityAccess failed: %d\n", err);
        exit(-1);
    }

    while (UDSClientPoll(&client));


    return 0;
}
