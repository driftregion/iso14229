/**
 * @file rtt_uds_example.c
 * @brief RT-Thread port and example for the iso14229 (UDS) server library.
 * @details This file provides a concrete implementation of a UDS server running on top of
 *          RT-Thread. It handles CAN communication, task management, and demonstrates
 *          a basic "Write Data By Identifier" service to control LEDs.
 *          It is designed to be controlled via the Finsh/MSH shell.
 *          This port is for the library found at: https://github.com/driftregion/iso14229
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2025-11-17 1.0     wdfk-prog   first version
 */

#include "iso14229.h"

#define DBG_TAG "isotp.rtt"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* --- Static Global Variables for State and Resource Management --- */

/** @brief Flag to indicate if the UDS service is currently active. */
static rt_bool_t is_running = RT_FALSE;

/** @brief RT-Thread device handle for the CAN peripheral. */
static rt_device_t can_dev = RT_NULL;

/** @brief Thread ID for the main UDS processing task. */
static rt_thread_t uds_task_tid = RT_NULL;

/** @brief Message queue for buffering incoming CAN frames from the ISR. */
static rt_mq_t can_rx_mq = RT_NULL;

/** @brief Stores the original CAN RX callback to restore it when the service stops. */
static rt_err_t (*old_can_rx_indicate)(rt_device_t dev, rt_size_t size) = RT_NULL;

/* --- Core UDS and ISO-TP Instances --- */

/** @brief The main UDS Server instance. */
static UDSServer_t srv;

/** @brief The ISO-TP-C instance for transport protocol handling. */
static UDSISOTpC_t tp;

/**
 * @brief ISO-TP configuration defining the CAN identifiers for communication.
 */
static const UDSISOTpCConfig_t tp_cfg = {
    .source_addr = 0x7E0, /**< Physical CAN ID this ECU listens to (ECU Request ID). */
    .target_addr = 0x7E8, /**< Physical CAN ID this ECU sends responses to (ECU Response ID). */
    .source_addr_func = 0x7DF,            /**< Functional broadcast CAN ID this ECU listens to. */
    .target_addr_func = UDS_TP_NOOP_ADDR, /**< Not used for a server. */
};

/**
 * @brief  Helper function to atomically print a title and hex data using ULOG.
 * @note   This function constructs a complete string in a temporary buffer before
 *         making a single call to LOG_D. This prevents log messages from being
 *         interleaved by other threads, which can happen if multiple logging
 *         calls are made for a single conceptual message.
 * @param  title A descriptive title for the hex data.
 * @param  data  Pointer to the data buffer to be printed.
 * @param  size  The size of the data in bytes.
 */
void print_hex_data(const char *title, const uint8_t *data, uint16_t size) {
    (void)title;
    (void)data;
    (void)size;
#if (DBG_LVL >= DBG_LOG)
    char log_buf[256];
    int offset = 0;

    offset +=
        rt_snprintf(log_buf + offset, sizeof(log_buf) - offset, "%s [%d bytes]:", title, size);

    for (uint16_t i = 0; i < size; i++) {
        if (offset >= sizeof(log_buf) - 4) {
            rt_snprintf(log_buf + offset, sizeof(log_buf) - offset, " ...");
            break;
        }
        offset += rt_snprintf(log_buf + offset, sizeof(log_buf) - offset, " %02X", data[i]);
    }

    LOG_D(log_buf);
#endif
}

/* -------------------------------------------------------------------------- */
/*                 User-Implemented Functions for isotp-c                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Platform-specific function to send a CAN frame.
 * @param  arbitration_id The CAN ID to send the frame with.
 * @param  data           Pointer to the data payload.
 * @param  size           Size of the data payload (up to 8 bytes).
 * @param  user_data      Optional user data pointer (not used here).
 * @return ISOTP_RET_OK on success, ISOTP_RET_ERROR on failure.
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    struct rt_can_msg msg = {0};

    msg.id = arbitration_id;
    msg.ide = RT_CAN_STDID;
    msg.rtr = RT_CAN_DTR;
    msg.len = size;
    rt_memcpy(msg.data, data, size);

#if (DBG_LVL >= DBG_LOG)
    char title_buf[32];
    rt_snprintf(title_buf, sizeof(title_buf), "[TX] ID: 0x%lX", arbitration_id);
    print_hex_data(title_buf, msg.data, size);
#endif

    if (rt_device_write(can_dev, 0, &msg, sizeof(msg)) == sizeof(msg)) {
        return ISOTP_RET_OK;
    }

    LOG_E("CAN send failed!");
    return ISOTP_RET_ERROR;
}

uint32_t isotp_user_get_us(void) {
    return (uint32_t)((rt_uint64_t)rt_tick_get() * 1000000 / RT_TICK_PER_SECOND);
}

/**
 * @brief  Platform-specific debug logging function.
 * @note   This function bridges the isotp-c library's internal logging with
 *         RT-Thread's ULOG system.
 * @param  format The format string for the log message.
 * @param  ...    Variable arguments for the format string.
 */
void isotp_user_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
#if defined(RT_USING_ULOG) && defined(ULOG_BACKEND_USING_CONSOLE)
    ulog_voutput(DBG_LVL, DBG_TAG, RT_TRUE, RT_NULL, 0, 0, 0, format, args);
#else
    char log_buf[128];
    rt_vsnprintf(log_buf, sizeof(log_buf), format, args);
    rt_kprintf("%s", log_buf);
#endif
    va_end(args);
}

/* -------------------------------------------------------------------------- */
/*                     UDS Server Event Callback Handling                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Main UDS server event callback.
 * @details This function is invoked by the UDS stack when a service request has been
 *          successfully received and parsed. It handles the application-specific
 *          logic for each supported UDS service.
 * @param  srv  Pointer to the UDS server instance.
 * @param  evt  The UDS event type that occurred (e.g., a specific service request).
 * @param  data A pointer to event-specific arguments. Must be cast to the correct type.
 * @return A UDS error code (UDSErr_t). UDS_PositiveResponse indicates success.
 *         Any other value will result in a Negative Response Code (NRC).
 */
static UDSErr_t server_callback(UDSServer_t *srv, UDSEvent_t evt, void *data) {
    LOG_I("Server Event: %s (0x%X)", UDSEventToStr(evt), evt);

    switch (evt) {
    case UDS_EVT_WriteDataByIdent: {
        UDSWDBIArgs_t *args = (UDSWDBIArgs_t *)data;

#if (DBG_LVL >= DBG_LOG)
        char title_buf[48];
        rt_snprintf(title_buf, sizeof(title_buf), "--> WDBI DID:0x%04X Data", args->dataId);
        print_hex_data(title_buf, args->data, args->len);
#endif
        // Check if the request is for the LED control DID.
        if (args->dataId == UDS_EXAMPLE_LED_CTRL_DID && args->len > 0) {
            uint8_t led_ctrl = args->data[0];
            RT_UNUSED(led_ctrl);
            LOG_I("Controlling LEDs with value: 0x%02X", led_ctrl);
#if (UDS_EXAMPLE_PIN_LED_R != -1)
            rt_pin_write(UDS_EXAMPLE_PIN_LED_R, (led_ctrl & 0x01) ? PIN_HIGH : PIN_LOW);
#endif
#if (UDS_EXAMPLE_PIN_LED_G != -1)
            rt_pin_write(UDS_EXAMPLE_PIN_LED_G, (led_ctrl & 0x02) ? PIN_HIGH : PIN_LOW);
#endif
#if (UDS_EXAMPLE_PIN_LED_B != -1)
            rt_pin_write(UDS_EXAMPLE_PIN_LED_B, (led_ctrl & 0x04) ? PIN_HIGH : PIN_LOW);
#endif
            return UDS_PositiveResponse;
        }
        // If the DID is not supported, send a negative response.
        return UDS_NRC_RequestOutOfRange;
    }

        // TODO: Add handlers for other UDS events here.

    default:
        // By default, if an event is not handled, return ServiceNotSupported.
        return UDS_NRC_ServiceNotSupported;
    }

    // This line should theoretically not be reached.
    return UDS_NRC_GeneralReject;
}

/* -------------------------------------------------------------------------- */
/*             CAN Message Producer-Consumer Model Implementation             */
/* -------------------------------------------------------------------------- */

/**
 * @brief  CAN device receive interrupt callback (Producer).
 * @details This function is called in an interrupt context whenever a CAN frame is
 *          received. It reads the message and pushes it into a message queue
 *          for deferred processing by the UDS task. This keeps ISR processing minimal.
 * @param  dev   The device that triggered the interrupt.
 * @param  size  The number of messages ready to be read.
 * @return RT_EOK on success.
 */
static rt_err_t can_rx_callback(rt_device_t dev, rt_size_t size) {
    struct rt_can_msg msg;
    // Key: Set hdr_index to -1 to receive messages from any hardware filter bank.
    msg.hdr_index = -1;

    if (rt_device_read(dev, 0, &msg, sizeof(msg)) == sizeof(msg)) {
        // Send the received message to the queue.
        if (rt_mq_send(can_rx_mq, &msg, sizeof(struct rt_can_msg)) == -RT_EFULL) {
            LOG_W("CAN RX message queue is full, message dropped.");
        }
    }
    return RT_EOK;
}

/**
 * @brief  Main UDS processing thread entry point (Consumer).
 * @details This thread waits for incoming CAN messages from the queue, forwards them
 *          to the appropriate ISO-TP link (physical or functional), and periodically
 *          polls the UDS server state machine.
 * @param  parameter Unused parameter.
 */
static void uds_task_entry(void *parameter) {
    struct rt_can_msg rx_msg;

    while (1) {
        if (rt_mq_recv(can_rx_mq, &rx_msg, sizeof(struct rt_can_msg),
                       rt_tick_from_millisecond(10)) == sizeof(struct rt_can_msg)) {
#if (DBG_LVL >= DBG_LOG)
            char title[32];
            rt_snprintf(title, sizeof(title), "CAN RX ID:0x%X", rx_msg.id);
            print_hex_data(title, rx_msg.data, rx_msg.len);
#endif

            // Feed the message to the appropriate ISO-TP link based on CAN ID.
            if (rx_msg.id == tp.phys_sa) // Physical addressing
            {
                isotp_on_can_message(&tp.phys_link, rx_msg.data, rx_msg.len);
            } else if (rx_msg.id == tp.func_sa) // Functional addressing
            {
                // Key: If a physical transfer is already in progress, ignore the functional frame.
                if (ISOTP_RECEIVE_STATUS_IDLE != tp.phys_link.receive_status) {
                    LOG_W("Functional frame received but physical link is busy, dropping.");
                    continue;
                }
                isotp_on_can_message(&tp.func_link, rx_msg.data, rx_msg.len);
            } else {
                LOG_W("Received unknown CAN ID 0x%03lX", rx_msg.id);
            }
        }

        UDSServerPoll(&srv);
    }
}

/* -------------------------------------------------------------------------- */
/*                         Finsh/MSH Control Commands                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Starts the UDS server example.
 * @details Initializes all required resources (CAN device, threads, message queues),
 *          configures hardware, and starts the UDS processing task.
 */
static void uds_start(void) {
    if (is_running) {
        rt_kprintf("UDS example is already running.\n");
        return;
    }

    can_dev = rt_device_find(UDS_EXAMPLE_CAN_DEVICE_NAME);
    if (!can_dev) {
        LOG_E("CAN device '%s' not found.", UDS_EXAMPLE_CAN_DEVICE_NAME);
        return;
    }

    old_can_rx_indicate = can_dev->rx_indicate;
    rt_device_close(can_dev);

    can_rx_mq = rt_mq_create("uds_rx_mq", sizeof(struct rt_can_msg), 32, RT_IPC_FLAG_FIFO);
    if (!can_rx_mq) {
        LOG_E("Failed to create can_rx_mq.");
        return;
    }

    uds_task_tid = rt_thread_create("uds_task", uds_task_entry, RT_NULL,
                                    UDS_EXAMPLE_THREAD_STACK_SIZE, UDS_EXAMPLE_THREAD_PRIO, 10);
    if (!uds_task_tid) {
        rt_mq_delete(can_rx_mq);
        LOG_E("Failed to create uds_task thread.");
        return;
    }
    rt_thread_startup(uds_task_tid);

    UDSServerInit(&srv);
    UDSISOTpCInit(&tp, &tp_cfg);
    srv.tp = &tp.hdl;
    srv.fn = server_callback;

    rt_device_open(can_dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX);

#ifdef RT_CAN_USING_HDR
    struct rt_can_filter_item items[] = {
        {
            .id = 0,                  // ID is irrelevant when mask is 0.
            .ide = RT_CAN_STDID,      // Apply to standard frames.
            .rtr = RT_CAN_DTR,        // Apply to data frames.
            .mode = RT_CAN_MODE_MASK, // Use mask mode.
            .mask = 0,                // Core: Mask of 0 accepts ALL IDs.
            .hdr_bank = -1,           // Let the driver allocate the filter bank.
        },
    };

    struct rt_can_filter_config cfg = {
        .count = 1,    // Number of filter entries
        .actived = 1,  // Activate this configuration
        .items = items // Pointing to our array of rules
    };

    rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &cfg);
#endif

    rt_device_control(can_dev, RT_CAN_CMD_SET_BAUD, (void *)CAN1MBaud);
    rt_device_control(can_dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_NORMAL);
    rt_device_set_rx_indicate(can_dev, can_rx_callback);

#if (UDS_EXAMPLE_PIN_LED_R != -1)
    rt_pin_mode(UDS_EXAMPLE_PIN_LED_R, PIN_MODE_OUTPUT);
#endif
#if (UDS_EXAMPLE_PIN_LED_G != -1)
    rt_pin_mode(UDS_EXAMPLE_PIN_LED_G, PIN_MODE_OUTPUT);
#endif
#if (UDS_EXAMPLE_PIN_LED_B != -1)
    rt_pin_mode(UDS_EXAMPLE_PIN_LED_B, PIN_MODE_OUTPUT);
#endif

    is_running = RT_TRUE;
    rt_device_control(can_dev, RT_CAN_CMD_START, &is_running);
    LOG_I("UDS example started on %s.", UDS_EXAMPLE_CAN_DEVICE_NAME);
}

/**
 * @brief Stops the UDS server example.
 * @details Deletes the processing thread and message queue, and restores the
 *          original CAN device configuration.
 */
static void uds_stop(void) {
    if (!is_running) {
        rt_kprintf("UDS example is not running.\n");
        return;
    }

    if (uds_task_tid)
        rt_thread_delete(uds_task_tid);
    if (can_rx_mq)
        rt_mq_delete(can_rx_mq);
    uds_task_tid = RT_NULL;
    can_rx_mq = RT_NULL;

    if (can_dev) {
        rt_device_set_rx_indicate(can_dev, old_can_rx_indicate);
        rt_device_close(can_dev);
        old_can_rx_indicate = RT_NULL;
    }

    is_running = RT_FALSE;
    LOG_I("UDS example stopped.");
}

/**
 * @brief Finsh/MSH command handler for the UDS example.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Always returns RT_EOK.
 */
static int uds_example(int argc, char **argv) {
    if (argc > 1) {
        if (!strcmp(argv[1], "start")) {
            uds_start();
        } else if (!strcmp(argv[1], "stop")) {
            uds_stop();
        } else {
            rt_kprintf("Usage: uds_example [start|stop]\n");
        }
    } else {
        rt_kprintf("Usage: uds_example [start|stop]\n");
    }
    return RT_EOK;
}
MSH_CMD_EXPORT(uds_example, UDS(ISO14229) server example);