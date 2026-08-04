#include "driver/gpio.h"
#include "iso14229.h"
#include <driver/twai.h>
#include <esp_log.h>
#include <esp_random.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>

#define CAN_RX_PIN GPIO_NUM_7
#define CAN_TX_PIN GPIO_NUM_6
#define RED_LED_PIN GPIO_NUM_3
#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5

const char *TAG = "UDS_0x27";

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_general_config_t g_config = {.mode = TWAI_MODE_NORMAL,
                                               .tx_io = CAN_TX_PIN,
                                               .rx_io = CAN_RX_PIN,
                                               .clkout_io = TWAI_IO_UNUSED,
                                               .bus_off_io = TWAI_IO_UNUSED,
                                               .tx_queue_len = 50,
                                               .rx_queue_len = 50,
                                               .alerts_enabled =
                                                   TWAI_ALERT_RX_DATA | TWAI_ALERT_BUS_OFF,
                                               .clkout_divider = 0,
                                               .intr_flags = ESP_INTR_FLAG_LEVEL1};

static UDSServer_t srv;
static UDSISOTpC_t tp;
static uint8_t seed[32] = {0};

// Embedded public key (this should match the private key used by clients)
// In production, this would be stored in secure storage
static const char public_key_pem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA2LGSuUBUx8yT5QJmvwX4\n"
    "v4WQx5L2M8N9h6K1r3D0s9F2a5P8c7B4e6Q3w1Z0y9X8v7U6t5S4r3Q2p1O0n9M8\n"
    "l7K6j5I4h3G2f1E0d9C8b7A6z5Y4x3W2v1U0t9S8r7Q6p5O4n3M2l1K0j9I8h7G6\n"
    "f5E4d3C2b1A0z9Y8x7W6v5U4t3S2r1Q0p9O8n7M6l5K4j3I2h1G0f9E8d7C6b5A4\n"
    "z3Y2x1W0v9U8t7S6r5Q4p3O2n1M0l9K8j7I6h5G4f3E2d1C0b9A8z7Y6x5W4v3U2\n"
    "t1S0r9Q8p7O6n5M4l3K2j1I0h9G8f7E6d5C4b3A2z1Y0x9W8v7U6t5S4r3Q2p1O0\n"
    "n9M8l7K6j5I4h3G2f1E0d9C8b7A6z5Y4x3W2v1U0t9S8r7Q6p5O4n3M2l1K0j9I8\n"
    "h7G6f5E4d3C2b1A0z9Y8x7W6v5U4t3S2r1Q0p9O8n7M6l5K4j3I2h1G0f9E8d7C6\n"
    "b5A4z3Y2x1W0v9U8t7S6r5Q4p3O2n1M0l9K8j7I6h5G4f3E2d1C0b9A8z7Y6x5W4\n"
    "v3U2t1S0r9Q8p7O6n5M4l3K2j1I0h9G8f7E6d5C4b3A2z1Y0x9W8v7U6t5S4r3Q2\n"
    "p1O0n9M8l7K6j5I4h3G2f1E0d9C8b7A6z5Y4x3W2v1U0t9S8r7Q6p5O4n3M2l1K0\n"
    "j9I8h7G6f5E4d3C2b1A0z9Y8x7W6v5U4t3S2r1Q0p9O8n7M6l5K4j3I2h1G0f9E8\n"
    "d7C6b5A4z3Y2x1W0v9U8t7S6r5Q4p3O2n1M0l9K8j7I6h5G4f3E2d1C0b9A8z7Y6\n"
    "x5W4v3U2t1S0r9Q8p7O6n5M4l3K2j1I0h9G8f7E6d5C4b3A2z1Y0x9W8v7U6t5S4\n"
    "r3Q2p1O0n9M8l7K6j5I4h3G2f1E0d9C8b7A6z5Y4x3W2v1U0t9S8r7Q6p5O4n3M2\n"
    "l1K0j9I8h7G6f5E4d3C2b1A0z9Y8x7W6v5U4t3S2r1Q0p9O8n7M6l5K4j3I2h1G0\n"
    "QIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    (void)user_data;
    twai_message_t tx_msg;
    tx_msg.identifier = arbitration_id;
    tx_msg.data_length_code = size;
    memmove(tx_msg.data, data, size);
    if (ESP_OK == twai_transmit(&tx_msg, 0)) {
        return size;
    } else {
        return -1;
    }
}

void isotp_user_debug(const char *fmt, ...) { (void)fmt; }

uint32_t isotp_user_get_us(void) { return UDSMillis() * 1000; }

static const UDSISOTpCConfig_t tp_cfg = {
    .source_addr = 0x7E8,
    .target_addr = 0x7E0,
    .source_addr_func = 0x7DF,
    .target_addr_func = UDS_TP_NOOP_ADDR,
};

int rsa_verify(const uint8_t *key, size_t key_len, bool *valid) {
    int ret = 0;
    mbedtls_pk_context pk;

    mbedtls_pk_init(&pk);

    // Parse the embedded public key
    if ((ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)public_key_pem,
                                           strlen(public_key_pem) + 1)) != 0) {
        ESP_LOGE(TAG, "Failed to parse public key, error: -0x%04x", -ret);
        goto exit;
    }

    // Verify the signature
    if ((ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, seed, sizeof(seed), key, key_len)) != 0) {
        ESP_LOGW(TAG, "Signature verification failed, error: -0x%04x", -ret);
        goto exit;
    }

    ESP_LOGI(TAG, "Signature verification successful");

exit:
    *valid = (ret == 0);
    mbedtls_pk_free(&pk);
    return ret;
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t evt, void *data) {
    switch (evt) {
    case UDS_EVT_SecAccessRequestSeed: {
        UDSSecAccessRequestSeedArgs_t *req = (UDSSecAccessRequestSeedArgs_t *)data;
        ESP_LOGI(TAG, "Security Access Request Seed for level %d", req->level);

        // Generate cryptographically secure random seed
        esp_fill_random(seed, sizeof(seed));

        // Log seed for debugging (in production, remove this!)
        ESP_LOG_BUFFER_HEX(TAG, seed, sizeof(seed));

        return req->copySeed(srv, seed, sizeof(seed));
    }
    case UDS_EVT_SecAccessValidateKey: {
        UDSSecAccessValidateKeyArgs_t *req = (UDSSecAccessValidateKeyArgs_t *)data;
        bool valid = false;

        ESP_LOGI(TAG, "Security Access Validate Key for level %d", req->level);
        ESP_LOG_BUFFER_HEX(TAG, req->key, req->len);

        if (0 != rsa_verify(req->key, req->len, &valid)) {
            ESP_LOGE(TAG, "RSA verification failed");
            return kGeneralReject;
        } else {
            if (valid) {
                ESP_LOGI(TAG, "Security level %d unlocked", req->level);
                // Visual indication of security unlock
                gpio_set_level(GREEN_LED_PIN, 1);
                return UDS_PositiveResponse;
            } else {
                ESP_LOGW(TAG, "Security access denied for level %d", req->level);
                // Visual indication of security failure
                gpio_set_level(RED_LED_PIN, 1);
                return kSecurityAccessDenied;
            }
        }
    }
    case UDS_EVT_WriteDataByIdent: {
        UDSWDBIArgs_t *r = (UDSWDBIArgs_t *)data;
        ESP_LOGI(TAG, "Write Data By Identifier: 0x%04x", r->dataId);
        switch (r->dataId) {
        case 0x0001:
            ESP_LOGI(TAG, "Setting LED state: 0x%02x", r->data[0]);
            gpio_set_level(RED_LED_PIN, r->data[0] & 0x01);
            gpio_set_level(GREEN_LED_PIN, r->data[0] & 0x02);
            gpio_set_level(BLUE_LED_PIN, r->data[0] & 0x04);
            break;
        default:
            ESP_LOGW(TAG, "Unknown data identifier: 0x%04x", r->dataId);
            return kRequestOutOfRange;
        }
        return UDS_PositiveResponse;
    }
    default:
        ESP_LOGW(TAG, "Unhandled UDS event: %d", evt);
        return kServiceNotSupported;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32 UDS Server with Security Access (0x27) support");

    // Initialize CAN driver
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "CAN driver initialized");

    // Configure GPIO for LEDs
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RED_LED_PIN) | (1ULL << GREEN_LED_PIN) | (1ULL << BLUE_LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Initialize all LEDs to off
    gpio_set_level(RED_LED_PIN, 0);
    gpio_set_level(GREEN_LED_PIN, 0);
    gpio_set_level(BLUE_LED_PIN, 0);

    ESP_LOGI(TAG, "GPIO configured");

    // Initialize UDS server
    ESP_ERROR_CHECK(UDSServerInit(&srv));
    ESP_ERROR_CHECK(UDSISOTpCInit(&tp, &tp_cfg));
    srv.fn = fn;
    srv.tp = &tp.hdl;

    ESP_LOGI(TAG, "UDS Server initialized, waiting for messages...");

    // Main loop
    for (;;) {
        twai_message_t rx_msg;
        if (twai_receive(&rx_msg, 0) == ESP_OK) {
            if (rx_msg.identifier == tp.phys_sa) {
                isotp_on_can_message(&tp.phys_link, rx_msg.data, rx_msg.data_length_code);
            } else if (rx_msg.identifier == tp.func_sa) {
                if (ISOTP_RECEIVE_STATUS_IDLE != tp.phys_link.receive_status) {
                    ESP_LOGD(TAG, "Functional frame received but physical link is busy");
                    continue;
                }
                isotp_on_can_message(&tp.func_link, rx_msg.data, rx_msg.data_length_code);
            }
        }

        UDSServerPoll(&srv);

        // Small delay to prevent watchdog timeout
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}