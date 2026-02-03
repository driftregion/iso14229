#ifndef AES_UTILS_H
#define AES_UTILS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Encrypt data using AES-128-ECB
 * @param key 16-byte AES key
 * @param input Input data to encrypt (16 bytes)
 * @param output Output buffer for encrypted data (16 bytes)
 * @return 0 on success, -1 on error
 */
int aes_encrypt_ecb(const uint8_t *key, const uint8_t *input, uint8_t *output);

/**
 * @brief Decrypt data using AES-128-ECB
 * @param key 16-byte AES key
 * @param input Input data to decrypt (16 bytes)
 * @param output Output buffer for decrypted data (16 bytes)
 * @return 0 on success, -1 on error
 */
int aes_decrypt_ecb(const uint8_t *key, const uint8_t *input, uint8_t *output);

/**
 * @brief Generate a random seed
 * @param seed Output buffer for seed (16 bytes)
 * @return 0 on success, -1 on error
 */
int generate_random_seed(uint8_t *seed);

#endif // AES_UTILS_H