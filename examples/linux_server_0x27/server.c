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
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/error.h>

static UDSServer_t srv;
static UDSTpIsoTpSock_t tp;
static bool done = false;
static uint8_t seed[32] = {0};

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

int rsa_verify(const uint8_t *key, size_t key_len, bool *valid) {
    int ret = 0;
    mbedtls_pk_context pk;
    const char *pubkey = "public_key.pem";
    mbedtls_pk_init(&pk);

    if ((ret = mbedtls_pk_parse_public_keyfile(&pk, pubkey) != 0)) {
        mbedtls_printf(" failed\n  ! Could not read key from '%s'\n", pubkey);
        mbedtls_printf("  ! mbedtls_pk_parse_public_keyfile returned %d\n\n", ret);
        goto exit;
    }

    if ((ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, seed, sizeof(seed),
                                 key, key_len)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_pk_verify returned %d\n\n", ret);
        goto exit;
    }

    exit:
    *valid = (ret == 0);
    // print the mbedtls error code
    if (ret != 0) {
        char buf[128];
        mbedtls_strerror(ret, buf, sizeof(buf));
        printf("mbedtls error: %s\n", buf);
    }
    return ret;
}

static uint8_t fn(UDSServer_t *srv, UDSEvent_t ev, const void *arg) {
    switch (ev) {
        case UDS_EVT_SecAccessRequestSeed: {
            UDSSecAccessRequestSeedArgs_t *req = (UDSSecAccessRequestSeedArgs_t *)arg;
            // use urandom to generate a random seed
            FILE *f = fopen("/dev/urandom", "r");
            if (!f) {
                fprintf(stderr, "Failed to open /dev/urandom\n");
                return kGeneralReject;
            }
            fread(seed, sizeof(seed), 1, f);
            fclose(f);
            return req->copySeed(srv, seed, sizeof(seed));
        }
        case UDS_EVT_SecAccessValidateKey: {
            UDSSecAccessValidateKeyArgs_t *req = (UDSSecAccessValidateKeyArgs_t *)arg;
            bool valid = false;

            if (0 != rsa_verify(req->key, req->len, &valid)) {
                printf("rsa_verify failed\n");
                return kGeneralReject;
            } else {
                if (valid) {
                    printf("Security level %d unlocked\n", req->level);
                    return UDS_PositiveResponse;
                } else {
                    return kSecurityAccessDenied;
                }
            }
        }
    default:
        printf("Unhandled event: %d\n", ev);
        return kServiceNotSupported;
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
