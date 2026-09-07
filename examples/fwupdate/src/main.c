#include <string.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/random/random.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/can.h>

#include <psa/crypto.h>

#include "iso14229.h"

/*
 * Private key:
 *   openssl genrsa -out security_access.pem 2048
 * Public key:
 *   openssl rsa -in security_access.pem -RSAPublicKey_out -outform DER -out public_key.der
 * To C array:
 *   python -c 'x=open("public_key.der", "rb").read(); print(", ".join([f"0x{i:02x}" for i in x]))'
 */
static const uint8_t public_key_der[] = {
    0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xe0, 0x7f, 0x71, 0xa0, 0xe9, 0xca, 0x0d,
    0x99, 0x16, 0x86, 0x9a, 0x07, 0xc8, 0x1d, 0x6a, 0x43, 0x9a, 0x97, 0x25, 0xfd, 0x07, 0xce, 0x1b,
    0x8e, 0x14, 0x91, 0x2a, 0xa9, 0xed, 0x44, 0x1e, 0x91, 0xaf, 0xcf, 0xbe, 0xb5, 0x70, 0xdc, 0xc0,
    0xc5, 0x35, 0x78, 0xb4, 0x9d, 0x5e, 0x6b, 0x0c, 0x02, 0x32, 0x98, 0x11, 0x76, 0xe9, 0x81, 0x12,
    0xf8, 0xcc, 0xd2, 0x65, 0x87, 0x26, 0x01, 0xaf, 0x26, 0x35, 0xce, 0xbd, 0xaf, 0x4d, 0xb2, 0xce,
    0xd4, 0x77, 0xca, 0x8e, 0x56, 0xf3, 0x8f, 0x80, 0xc9, 0x5c, 0x45, 0xfe, 0xa8, 0xf0, 0xde, 0xaa,
    0xb5, 0x68, 0x99, 0xaf, 0x16, 0xdc, 0x38, 0x6c, 0x24, 0x2c, 0x23, 0x74, 0x0a, 0x12, 0xc6, 0x3f,
    0x6f, 0x6b, 0xb2, 0xb3, 0x67, 0x9c, 0x55, 0xa3, 0xec, 0x51, 0x52, 0x54, 0xa7, 0xfb, 0x19, 0xd0,
    0xe4, 0x68, 0xbe, 0x32, 0xb1, 0x16, 0x61, 0x49, 0x46, 0x02, 0x82, 0x39, 0xa4, 0x1a, 0xf9, 0x3a,
    0xdb, 0x88, 0xcb, 0x06, 0xde, 0xc1, 0x7c, 0xd3, 0x52, 0x2f, 0x5d, 0xe8, 0x67, 0xc4, 0x0a, 0x72,
    0x18, 0x22, 0x83, 0x08, 0xb1, 0xaa, 0xd4, 0xa3, 0x9e, 0x87, 0x23, 0x86, 0xad, 0x9e, 0xda, 0x38,
    0xcb, 0x8d, 0xa5, 0x1b, 0xc9, 0x54, 0x25, 0xc6, 0xca, 0xd6, 0x34, 0x36, 0x2c, 0x6d, 0x6e, 0x49,
    0x72, 0x11, 0x71, 0x94, 0xec, 0xb5, 0x6a, 0x54, 0x6f, 0x69, 0xef, 0xf4, 0x0b, 0xab, 0xcb, 0x8d,
    0xb2, 0x69, 0x7a, 0x00, 0xc0, 0xb3, 0x84, 0x37, 0x1e, 0xeb, 0x6a, 0x2e, 0xb8, 0xde, 0x47, 0xc8,
    0x7f, 0x69, 0x8e, 0x24, 0x4c, 0xc8, 0xe7, 0x30, 0x92, 0xbd, 0xa8, 0xf5, 0xdd, 0xaa, 0x4e, 0x93,
    0xac, 0x2f, 0x13, 0x79, 0xae, 0xf4, 0xbe, 0x36, 0x5d, 0x79, 0x1a, 0x92, 0x7f, 0xed, 0x99, 0xba,
    0x93, 0x59, 0xa9, 0x6f, 0x02, 0x67, 0x8b, 0xd5, 0x7b, 0x02, 0x03, 0x01, 0x00, 0x01,
};

static mbedtls_svc_key_id_t public_key_id;
static bool security_unlocked;

#define PHYS_RX_ADDR 0x7E0
#define PHYS_TX_ADDR 0x7E8
#define FUNC_RX_ADDR 0x7DF

CAN_MSGQ_DEFINE(can_rxq, 16);

static const struct device *can_dev;

uint32_t isotp_user_get_us(void) { return k_uptime_get_32() * 1000; }

void isotp_user_debug(const char *message, ...) { (void)message; }

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    (void)user_data;
    struct can_frame frame = {.id = arbitration_id, .dlc = size};
    memcpy(frame.data, data, size);
    if (can_send(can_dev, &frame, K_MSEC(100), NULL, NULL)) {
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}

#define DID_ACTIVE_IMAGE_VERSION 0xF100
#define DID_FINGERPRINT 0xF184
#define ROUTINE_CHECK_PROGRAMMING_DEPENDENCIES 0xFF01

static uint8_t last_seed[4];
static uint8_t fingerprint[16];
static uint16_t fingerprint_len;

static struct flash_img_context flash_img_ctx;
static size_t transfer_expected_size;

static UDSErr_t on_read_data_by_ident(UDSServer_t *srv, UDSRDBIArgs_t *r) {
    switch (r->dataId) {
    case DID_ACTIVE_IMAGE_VERSION: {
        struct mcuboot_img_header hdr;
        char buf[24];

        if (boot_read_bank_header(PARTITION_ID(slot0_partition), &hdr, sizeof(hdr))) {
            return UDS_NRC_ConditionsNotCorrect;
        }
        snprintf(buf, sizeof(buf), "%u.%u.%u+%u", hdr.h.v1.sem_ver.major, hdr.h.v1.sem_ver.minor,
                 hdr.h.v1.sem_ver.revision, hdr.h.v1.sem_ver.build_num);
        return r->copy(srv, buf, strlen(buf));
    }
    default:
        return UDS_NRC_RequestOutOfRange;
    }
}

static UDSErr_t on_write_data_by_ident(UDSWDBIArgs_t *r) {
    switch (r->dataId) {
    case DID_FINGERPRINT:
        if (r->len > sizeof(fingerprint)) {
            return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        }
        memcpy(fingerprint, r->data, r->len);
        fingerprint_len = r->len;
        return UDS_PositiveResponse;
    default:
        return UDS_NRC_RequestOutOfRange;
    }
}

static UDSErr_t on_routine_ctrl(UDSRoutineCtrlArgs_t *r) {
    switch (r->id) {
    case ROUTINE_CHECK_PROGRAMMING_DEPENDENCIES:
        if (boot_request_upgrade(BOOT_UPGRADE_TEST)) {
            return UDS_NRC_GeneralProgrammingFailure;
        }
        return UDS_PositiveResponse;
    default:
        return UDS_NRC_RequestOutOfRange;
    }
}

static UDSErr_t on_request_download(UDSRequestDownloadArgs_t *r) {
    if (!security_unlocked) {
        return UDS_NRC_SecurityAccessDenied;
    }
    if (flash_img_init(&flash_img_ctx)) {
        return UDS_NRC_GeneralProgrammingFailure;
    }
    transfer_expected_size = r->size;
    r->maxNumberOfBlockLength = 256;
    return UDS_PositiveResponse;
}

static UDSErr_t on_transfer_data(UDSTransferDataArgs_t *r) {
    if (flash_img_buffered_write(&flash_img_ctx, r->data, r->len, false)) {
        return UDS_NRC_GeneralProgrammingFailure;
    }
    return UDS_PositiveResponse;
}

static UDSErr_t on_request_transfer_exit(void) {
    if (flash_img_buffered_write(&flash_img_ctx, NULL, 0, true)) {
        return UDS_NRC_GeneralProgrammingFailure;
    }
    if (flash_img_bytes_written(&flash_img_ctx) != transfer_expected_size) {
        return UDS_NRC_GeneralProgrammingFailure;
    }
    return UDS_PositiveResponse;
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    switch (ev) {
    case UDS_EVT_DiagSessCtrl: {
        UDSDiagSessCtrlArgs_t *r = (UDSDiagSessCtrlArgs_t *)arg;
        switch (r->type) {
        case UDS_LEV_DS_PRGS:
            r->p2_ms = 1000;
            r->p2_star_ms = 5000;
            break;
        default:
            break;
        }
        return UDS_PositiveResponse;
    }
    case UDS_EVT_EcuReset:
    case UDS_EVT_CommCtrl:
    case UDS_EVT_ControlDTCSetting:
        return UDS_PositiveResponse;
    case UDS_EVT_DoScheduledReset:
        sys_reboot(SYS_REBOOT_WARM);
        return UDS_OK;
    case UDS_EVT_ReadDataByIdent:
        return on_read_data_by_ident(srv, (UDSRDBIArgs_t *)arg);
    case UDS_EVT_WriteDataByIdent:
        return on_write_data_by_ident((UDSWDBIArgs_t *)arg);
    case UDS_EVT_SecAccessRequestSeed: {
        UDSSecAccessRequestSeedArgs_t *r = (UDSSecAccessRequestSeedArgs_t *)arg;
        if (sys_csrand_get(last_seed, sizeof(last_seed))) {
            return UDS_NRC_GeneralReject;
        }
        return r->copySeed(srv, last_seed, sizeof(last_seed));
    }
    case UDS_EVT_SecAccessValidateKey: {
        UDSSecAccessValidateKeyArgs_t *r = (UDSSecAccessValidateKeyArgs_t *)arg;
        uint8_t hash[32];
        size_t hash_len;
        if (psa_hash_compute(PSA_ALG_SHA_256, last_seed, sizeof(last_seed), hash, sizeof(hash),
                             &hash_len)) {
            return UDS_NRC_GeneralReject;
        }
        if (psa_verify_hash(public_key_id, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256), hash,
                            hash_len, r->key, r->len) != PSA_SUCCESS) {
            return UDS_NRC_InvalidKey;
        }
        security_unlocked = true;
        return UDS_PositiveResponse;
    }
    case UDS_EVT_RoutineCtrl:
        return on_routine_ctrl((UDSRoutineCtrlArgs_t *)arg);
    case UDS_EVT_RequestDownload:
        return on_request_download((UDSRequestDownloadArgs_t *)arg);
    case UDS_EVT_TransferData:
        return on_transfer_data((UDSTransferDataArgs_t *)arg);
    case UDS_EVT_RequestTransferExit:
        return on_request_transfer_exit();
    default:
        return UDS_NRC_ServiceNotSupported;
    }
}

int main(void) {
    static UDSServer_t srv;
    static UDSTpISOTpC_t tp;

    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!device_is_ready(can_dev) || can_start(can_dev)) {
        return -1;
    }

    if (psa_crypto_init() != PSA_SUCCESS) {
        return -1;
    }
    psa_key_attributes_t public_key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&public_key_attr, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
    psa_set_key_usage_flags(&public_key_attr, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&public_key_attr, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));
    if (psa_import_key(&public_key_attr, public_key_der, sizeof(public_key_der), &public_key_id) !=
        PSA_SUCCESS) {
        return -1;
    }

    UDSServerTpISOTpCInit(&tp, PHYS_RX_ADDR, PHYS_TX_ADDR, FUNC_RX_ADDR);

    const struct can_filter phys_filter = {.id = PHYS_RX_ADDR, .mask = CAN_STD_ID_MASK};
    const struct can_filter func_filter = {.id = FUNC_RX_ADDR, .mask = CAN_STD_ID_MASK};
    can_add_rx_filter_msgq(can_dev, &can_rxq, &phys_filter);
    can_add_rx_filter_msgq(can_dev, &can_rxq, &func_filter);

    UDSServerInit(&srv);
    srv.tp = &tp.hdl;
    srv.fn = fn;

    if (!boot_is_img_confirmed()) {
        boot_write_img_confirmed();
    }

    printk("fwupdate example app booted on %s, slot %s\n", CONFIG_BOARD,
           DT_PROP(DT_CHOSEN(zephyr_code_partition), label));

    struct can_frame frame;
    while (1) {
        while (k_msgq_get(&can_rxq, &frame, K_NO_WAIT) == 0) {
            if (frame.id == PHYS_RX_ADDR) {
                isotp_on_can_message(&tp.phys_link, frame.data, frame.dlc);
            } else if (frame.id == FUNC_RX_ADDR) {
                isotp_on_can_message(&tp.func_link, frame.data, frame.dlc);
            }
        }
        UDSServerPoll(&srv);
        k_msleep(1);
    }
    return 0;
}
