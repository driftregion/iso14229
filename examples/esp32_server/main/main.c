#include "driver/gpio.h"
#include "iso14229.h"
#include <driver/twai.h>
#include <esp_log.h>

#define CAN_RX_PIN           GPIO_NUM_7
#define CAN_TX_PIN           GPIO_NUM_6
#define RED_LED_PIN          GPIO_NUM_3
#define GREEN_LED_PIN        GPIO_NUM_4
#define BLUE_LED_PIN         GPIO_NUM_5

const char *TAG = "UDS";

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

void isotp_user_debug(const char *fmt, ...) {
  (void)fmt;
}

uint32_t isotp_user_get_us(void) { return UDSMillis() * 1000; }


static const UDSISOTpCConfig_t tp_cfg = {
    .source_addr=0x7E8,
    .target_addr=0x7E0,
    .source_addr_func=0x7DF,
    .target_addr_func=UDS_TP_NOOP_ADDR,
};

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t evt, void *data) {
    ESP_LOGI(TAG, "received event %d", evt);
    switch (evt) {
        case UDS_EVT_WriteDataByIdent: {
            UDSWDBIArgs_t *r = (UDSWDBIArgs_t *)data;
            switch (r->dataId) {
                case 0x0001:
                    ESP_LOGI(TAG, "received 0x0001");
                    gpio_set_level(RED_LED_PIN, r->data[0] & 0x01);
                    gpio_set_level(GREEN_LED_PIN, r->data[0] & 0x02);
                    gpio_set_level(BLUE_LED_PIN, r->data[0] & 0x04);
                    break;
                default:
                    ESP_LOGI(TAG, "received unknown data id 0x%04x", r->dataId);
                    break;
            }
            break;
        }
        default:
            ESP_LOGI(TAG, "unhandled event %d", evt);
            break;
    }
    return 0;
}

void app_main(void) {
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
    
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RED_LED_PIN) | (1ULL << GREEN_LED_PIN) | (1ULL << BLUE_LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);


    ESP_ERROR_CHECK(UDSServerInit(&srv));
    ESP_ERROR_CHECK(UDSISOTpCInit(&tp, &tp_cfg));
    srv.fn = fn;
    srv.tp = &tp.hdl;

    for (;;) {
        twai_message_t rx_msg;
        if (twai_receive(&rx_msg, 0) == ESP_OK) {
            if (rx_msg.identifier == tp.phys_sa) {
                isotp_on_can_message(&tp.phys_link, rx_msg.data, rx_msg.data_length_code);
            } else if (rx_msg.identifier == tp.func_sa) {
                if (ISOTP_RECEIVE_STATUS_IDLE != tp.phys_link.receive_status) {
                    ESP_LOGI(TAG, "func frame received but cannot process because link is not idle");
                    continue;
                }
                isotp_on_can_message(&tp.func_link, rx_msg.data, rx_msg.data_length_code);
            } else {
                ESP_LOGI(TAG, "received unknown can id 0x%03lx", rx_msg.identifier);
            }
        }

        UDSServerPoll(&srv);
    }

}