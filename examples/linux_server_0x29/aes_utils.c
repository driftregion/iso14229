#include "aes_utils.h"
#include <mbedtls/aes.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <string.h>

int aes_encrypt_ecb(const uint8_t *key, const uint8_t *input, uint8_t *output) {
    mbedtls_aes_context aes;
    int ret;

    mbedtls_aes_init(&aes);
    ret = mbedtls_aes_setkey_enc(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return -1;
    }

    ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, output);
    mbedtls_aes_free(&aes);

    return (ret == 0) ? 0 : -1;
}

int aes_decrypt_ecb(const uint8_t *key, const uint8_t *input, uint8_t *output) {
    mbedtls_aes_context aes;
    int ret;

    mbedtls_aes_init(&aes);
    ret = mbedtls_aes_setkey_dec(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return -1;
    }

    ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input, output);
    mbedtls_aes_free(&aes);

    return (ret == 0) ? 0 : -1;
}

int generate_random_seed(uint8_t *seed) {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *personalization = "uds_seed_gen";
    int ret;

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)personalization, strlen(personalization));
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return -1;
    }

    ret = mbedtls_ctr_drbg_random(&ctr_drbg, seed, 16);

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return (ret == 0) ? 0 : -1;
}