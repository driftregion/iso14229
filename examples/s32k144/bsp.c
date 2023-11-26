#include "pins_driver.h"
#include "clock.h"
#include "edma_driver.h"
#include "flexcan_driver.h"
#include "system_S32K144.h"

/*! @brief User number of configured pins */
#define NUM_OF_CONFIGURED_PINS 12

/*! @brief User configuration structure */
const static pin_settings_config_t g_pin_mux_InitConfigArr[NUM_OF_CONFIGURED_PINS] = {
    {
        .base = PORTD,
        .pinPortIdx = 0u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTD,
        .direction = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue = 0u,
    },
    {
        .base = PORTE,
        .pinPortIdx = 5u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_ALT5,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = NULL,
        .digitalFilter = false,
    },
    {
        .base = PORTE,
        .pinPortIdx = 4u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_ALT5,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = NULL,
        .digitalFilter = false,
    },
    {
        .base = PORTD,
        .pinPortIdx = 16u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTD,
        .direction = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue = 0u,
    },
    {
        .base = PORTD,
        .pinPortIdx = 15u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTD,
        .direction = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue = 0u,
    },
    {
        .base = PORTC,
        .pinPortIdx = 3u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTC,
        .direction = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue = 0u,
    },
    {
        .base = PORTC,
        .pinPortIdx = 2u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTC,
        .direction = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue = 0u,
    },
    {
        .base = PORTC,
        .pinPortIdx = 1u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTC,
        .direction = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue = 0u,
    },
    {
        .base = PORTC,
        .pinPortIdx = 0u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTC,
        .direction = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue = 0u,
    },
    {
        .base = PORTC,
        .pinPortIdx = 14u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_PIN_DISABLED,
        .pinLock = false,
        .intConfig = PORT_INT_RISING_EDGE,
        .clearIntFlag = false,
        .gpioBase = NULL,
        .digitalFilter = false,
    },
    {
        .base = PORTC,
        .pinPortIdx = 13u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_INT_RISING_EDGE,
        .clearIntFlag = false,
        .gpioBase = PTC,
        .direction = GPIO_INPUT_DIRECTION,
        .digitalFilter = false,
    },
    {
        .base = PORTC,
        .pinPortIdx = 12u,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_MUX_AS_GPIO,
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = PTC,
        .direction = GPIO_INPUT_DIRECTION,
        .digitalFilter = false,
    },
};

#define LED_PORT PORTD
#define GPIO_PORT PTD
#define PCC_INDEX PCC_PORTD_INDEX
#define LED0 15U
#define LED1 16U
#define LED2 0U

#define BTN_GPIO PTC
#define BTN1_PIN 13U
#define BTN2_PIN 12U
#define BTN_PORT PORTC
#define BTN_PORT_IRQn PORTC_IRQn

/*
 * @brief Function which configures the LEDs and Buttons
 */
static void GPIOInit(void) {
    /* Output direction for LEDs */
    PINS_DRV_SetPinsDirection(GPIO_PORT, (1 << LED2) | (1 << LED1) | (1 << LED0));

    /* Set Output value LEDs */
    // PINS_DRV_ClearPins(GPIO_PORT, 1 << LED1);
    PINS_DRV_SetPins(GPIO_PORT, (1 << LED2) | (1 << LED1) | (1 << LED0));

    /* Setup button pin */
    PINS_DRV_SetPinsDirection(BTN_GPIO, ~((1 << BTN1_PIN) | (1 << BTN2_PIN)));

    /* Setup button pins interrupt */
    PINS_DRV_SetPinIntSel(BTN_PORT, BTN1_PIN, PORT_INT_RISING_EDGE);
    PINS_DRV_SetPinIntSel(BTN_PORT, BTN2_PIN, PORT_INT_RISING_EDGE);

    // /* Install buttons ISR */
    // INT_SYS_InstallHandler(BTN_PORT_IRQn, &buttonISR, NULL);

    // /* Enable buttons interrupt */
    // INT_SYS_EnableIRQ(BTN_PORT_IRQn);
}

#define INST_CANCOM1 (0U)
static flexcan_state_t canCom1_State;

const flexcan_user_config_t canCom1_InitConfig0 = {
    .fd_enable = true,
    .pe_clock = FLEXCAN_CLK_SOURCE_OSC,
    .max_num_mb = 16,
    .num_id_filters = FLEXCAN_RX_FIFO_ID_FILTERS_8,
    .is_rx_fifo_needed = false,
    .flexcanMode = FLEXCAN_NORMAL_MODE,
    .payload = FLEXCAN_PAYLOAD_SIZE_16,
    .bitrate = {.propSeg = 7, .phaseSeg1 = 4, .phaseSeg2 = 1, .preDivider = 0, .rJumpwidth = 1},
    .bitrate_cbt = {.propSeg = 7, .phaseSeg1 = 4, .phaseSeg2 = 1, .preDivider = 0, .rJumpwidth = 1},
    .transfer_type = FLEXCAN_RXFIFO_USING_INTERRUPTS,
    .rxFifoDMAChannel = 0U};

#define TX_MAILBOX (1UL)
#define TX_MSG_ID (1UL)
#define RX_MAILBOX (0UL)
#define RX_MSG_ID (2UL)
static flexcan_msgbuff_t recvBuff;
typedef enum { LED0_CHANGE_REQUESTED = 0x00U, LED1_CHANGE_REQUESTED = 0x01U } can_commands_list;

uint8_t ledRequested = (uint8_t)LED0_CHANGE_REQUESTED;
uint8_t callback_test = 0;

void flexcan0_Callback(uint8_t instance, flexcan_event_type_t eventType, uint32_t buffIdx,
                       flexcan_state_t *flexcanState) {

    (void)flexcanState;
    (void)instance;

    switch (eventType) {
    case FLEXCAN_EVENT_RX_COMPLETE:
        callback_test |= 0x1; // set bit0 to to evidence RX was complete
        if (buffIdx == RX_MAILBOX) {
            if ((recvBuff.data[0] == LED0_CHANGE_REQUESTED) && recvBuff.msgId == RX_MSG_ID) {
                /* Toggle output value LED1 */
                PINS_DRV_TogglePins(GPIO_PORT, (1 << LED0));
            } else if ((recvBuff.data[0] == LED1_CHANGE_REQUESTED) && recvBuff.msgId == RX_MSG_ID) {
                /* Toggle output value LED0 */
                PINS_DRV_TogglePins(GPIO_PORT, (1 << LED1));
            }
            /* Start receiving data in RX_MAILBOX again. */
            FLEXCAN_DRV_Receive(INST_CANCOM1, RX_MAILBOX, &recvBuff);
        }

        break;
    case FLEXCAN_EVENT_RXFIFO_COMPLETE:
        break;
    case FLEXCAN_EVENT_DMA_COMPLETE:
        break;
    case FLEXCAN_EVENT_TX_COMPLETE:
        callback_test |= 0x2; // set bit1 to to evidence TX was complete
        PINS_DRV_SetPins(GPIO_PORT, 1 << LED2);
        break;
    default:
        break;
    }
}

void flexcan0_ErrorCallback(uint8_t instance, flexcan_event_type_t eventType,
                            flexcan_state_t *flexcanState) {
    volatile uint32_t error;

    (void)flexcanState;
    (void)instance;

    switch (eventType) {
    case FLEXCAN_EVENT_ERROR:
        callback_test |= 0x4; // set bit2 to to evidence error ISR hit

        error = FLEXCAN_DRV_GetErrorStatus(INST_CANCOM1);

        if (error & 0x4) // if BOFFINT was set
        {
            callback_test |= 0x8; // set bit3 to to evidence bus off ISR hit

            // abort TX MB, after bus off recovery message is not send
            FLEXCAN_DRV_AbortTransfer(INST_CANCOM1, TX_MAILBOX);

            PINS_DRV_ClearPins(GPIO_PORT, 1 << LED2);
        }

        break;

    default:
        break;
    }
}

/*
 * @brief: Send data via CAN to the specified mailbox with the specified message id
 * @param mailbox   : Destination mailbox number
 * @param messageId : Message ID
 * @param data      : Pointer to the TX data
 * @param len       : Length of the TX data
 * @return          : None
 */
status_t SendCANData(uint32_t mailbox, uint32_t messageId, uint8_t *data, uint32_t len) {
    /* Set information about the data to be sent
     *  - 1 byte in length
     *  - Standard message ID
     *  - Bit rate switch enabled to use a different bitrate for the data segment
     *  - Flexible data rate enabled
     *  - Use zeros for FD padding
     */
    flexcan_data_info_t dataInfo = {.data_length = len,
                                    .msg_id_type = FLEXCAN_MSG_ID_STD,
                                    .enable_brs = true,
                                    .fd_enable = false,
                                    .fd_padding = 0U};

    /* Configure TX message buffer with index TX_MSG_ID and TX_MAILBOX*/
    FLEXCAN_DRV_ConfigTxMb(INST_CANCOM1, mailbox, &dataInfo, messageId);

    /* Execute send non-blocking */
    status_t status = FLEXCAN_DRV_Send(INST_CANCOM1, mailbox, &dataInfo, messageId, data);
    return status;
}

int BSPSendCAN(uint32_t id, uint8_t *data, uint32_t len) {
    return SendCANData(TX_MAILBOX, id, data, len);
}

/*
 * @brief Initialize FlexCAN driver and configure the bit rate
 */
void FlexCANInit(void) {
    /*
     * Initialize FlexCAN driver
     *  - 8 byte payload size
     *  - FD enabled
     *  - Bus clock as peripheral engine clock
     */
    status_t status = FLEXCAN_DRV_Init(INST_CANCOM1, &canCom1_State, &canCom1_InitConfig0);
    DEV_ASSERT(STATUS_SUCCESS == status);
    FLEXCAN_DRV_InstallEventCallback(INST_CANCOM1, (flexcan_callback_t)flexcan0_Callback,
                                     (void *)NULL);
    FLEXCAN_DRV_InstallErrorCallback(INST_CANCOM1, (flexcan_error_callback_t)flexcan0_ErrorCallback,
                                     (void *)NULL);
}

/*! @brief Count of user configuration structures */
#define CLOCK_MANAGER_CONFIG_CNT 1U

/*! @brief Count of peripheral clock user configurations */
#define NUM_OF_PERIPHERAL_CLOCKS_0 13U

/*! @brief Count of user Callbacks */
#define CLOCK_MANAGER_CALLBACK_CNT 0U

peripheral_clock_config_t peripheralClockConfig0[NUM_OF_PERIPHERAL_CLOCKS_0] = {
    {
        .clockName = DMAMUX0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FlexCAN0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FlexCAN1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FlexCAN2_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FTFC0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPSPI0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPSPI1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPSPI2_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTA_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTB_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTC_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTD_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTE_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
};

/* *************************************************************************
 * Configuration structure for Clock Configuration 0
 * ************************************************************************* */
/*! @brief User Configuration structure clockMan1_InitConfig0 */
clock_manager_user_config_t clockMan1_InitConfig0 = {
    /*! @brief Configuration of SIRC */
    .scgConfig =
        {
            .sircConfig =
                {
                    .initialize = true, /*!< Initialize */
                    /* SIRCCSR */
                    .enableInStop = true,     /*!< SIRCSTEN  */
                    .enableInLowPower = true, /*!< SIRCLPEN  */
                    .locked = false,          /*!< LK        */
                    /* SIRCCFG */
                    .range = SCG_SIRC_RANGE_HIGH, /*!< RANGE - High range (8 MHz) */
                    /* SIRCDIV */
                    .div1 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< SIRCDIV1  */
                    .div2 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< SIRCDIV2  */
                },
            .fircConfig =
                {
                    .initialize = true, /*!< Initialize */
                    /* FIRCCSR */
                    .regulator = true, /*!< FIRCREGOFF */
                    .locked = false,   /*!< LK         */
                    /* FIRCCFG */
                    .range = SCG_FIRC_RANGE_48M, /*!< RANGE      */
                    /* FIRCDIV */
                    .div1 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< FIRCDIV1   */
                    .div2 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< FIRCDIV2   */
                },
            .rtcConfig =
                {
                    .initialize = true, /*!< Initialize  */
                    .rtcClkInFreq = 0U, /*!< RTC_CLKIN   */
                },
            .soscConfig =
                {
                    .initialize = true, /*!< Initialize */
                    .freq = 8000000U,   /*!< Frequency  */
                    /* SOSCCSR */
                    .monitorMode = SCG_SOSC_MONITOR_DISABLE, /*!< SOSCCM      */
                    .locked = false,                         /*!< LK          */
                    /* SOSCCFG */
                    .extRef = SCG_SOSC_REF_OSC,   /*!< EREFS       */
                    .gain = SCG_SOSC_GAIN_LOW,    /*!< HGO         */
                    .range = SCG_SOSC_RANGE_HIGH, /*!< RANGE       */
                    /* SOSCDIV */
                    .div1 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< SOSCDIV1    */
                    .div2 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< SOSCDIV2    */
                },
            .spllConfig =
                {
                    .initialize = true, /*!< Initialize */
                    /* SPLLCSR */
                    .monitorMode = SCG_SPLL_MONITOR_DISABLE, /*!< SPLLCM     */
                    .locked = false,                         /*!< LK         */
                    /* SPLLCFG */
                    .prediv = (uint8_t)SCG_SPLL_CLOCK_PREDIV_BY_1,  /*!< PREDIV     */
                    .mult = (uint8_t)SCG_SPLL_CLOCK_MULTIPLY_BY_28, /*!< MULT       */
                    .src = 0U,                                      /*!< SOURCE     */
                    /* SPLLDIV */
                    .div1 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< SPLLDIV1   */
                    .div2 = SCG_ASYNC_CLOCK_DIV_BY_1, /*!< SPLLDIV2   */
                },
            .clockOutConfig =
                {
                    .initialize = true,              /*!< Initialize    */
                    .source = SCG_CLOCKOUT_SRC_FIRC, /*!< SCG CLKOUTSEL     */
                },
            .clockModeConfig =
                {
                    .initialize = true, /*!< Initialize */
                    .rccrConfig =       /*!< RCCR - Run Clock Control Register          */
                    {
                        .src = SCG_SYSTEM_CLOCK_SRC_FIRC,     /*!< SCS        */
                        .divCore = SCG_SYSTEM_CLOCK_DIV_BY_1, /*!< DIVCORE    */
                        .divBus = SCG_SYSTEM_CLOCK_DIV_BY_2,  /*!< DIVBUS     */
                        .divSlow = SCG_SYSTEM_CLOCK_DIV_BY_2, /*!< DIVSLOW    */
                    },
                    .vccrConfig = /*!< VCCR - VLPR Clock Control Register         */
                    {
                        .src = SCG_SYSTEM_CLOCK_SRC_SIRC,     /*!< SCS        */
                        .divCore = SCG_SYSTEM_CLOCK_DIV_BY_2, /*!< DIVCORE    */
                        .divBus = SCG_SYSTEM_CLOCK_DIV_BY_1,  /*!< DIVBUS     */
                        .divSlow = SCG_SYSTEM_CLOCK_DIV_BY_4, /*!< DIVSLOW    */
                    },
                    .hccrConfig = /*!< HCCR - HSRUN Clock Control Register        */
                    {
                        .src = SCG_SYSTEM_CLOCK_SRC_SYS_PLL,  /*!< SCS        */
                        .divCore = SCG_SYSTEM_CLOCK_DIV_BY_1, /*!< DIVCORE    */
                        .divBus = SCG_SYSTEM_CLOCK_DIV_BY_2,  /*!< DIVBUS     */
                        .divSlow = SCG_SYSTEM_CLOCK_DIV_BY_4, /*!< DIVSLOW    */
                    },
                },
        },
    .pccConfig =
        {
            .peripheralClocks =
                peripheralClockConfig0,          /*!< Peripheral clock control configurations  */
            .count = NUM_OF_PERIPHERAL_CLOCKS_0, /*!< Number of the peripheral clock control
                                                    configurations  */
        },
    .simConfig =
        {
            .clockOutConfig = /*!< Clock Out configuration.           */
            {
                .initialize = true,                         /*!< Initialize    */
                .enable = false,                            /*!< CLKOUTEN      */
                .source = SIM_CLKOUT_SEL_SYSTEM_SCG_CLKOUT, /*!< CLKOUTSEL     */
                .divider = SIM_CLKOUT_DIV_BY_1,             /*!< CLKOUTDIV     */
            },
            .lpoClockConfig = /*!< Low Power Clock configuration.     */
            {
                .initialize = true,                          /*!< Initialize    */
                .enableLpo1k = true,                         /*!< LPO1KCLKEN    */
                .enableLpo32k = true,                        /*!< LPO32KCLKEN   */
                .sourceLpoClk = SIM_LPO_CLK_SEL_LPO_128K,    /*!< LPOCLKSEL     */
                .sourceRtcClk = SIM_RTCCLK_SEL_SOSCDIV1_CLK, /*!< RTCCLKSEL     */
            },
            .platGateConfig = /*!< Platform Gate Clock configuration. */
            {
                .initialize = true, /*!< Initialize    */
                .enableMscm = true, /*!< CGCMSCM       */
                .enableMpu = true,  /*!< CGCMPU        */
                .enableDma = true,  /*!< CGCDMA        */
                .enableErm = true,  /*!< CGCERM        */
                .enableEim = true,  /*!< CGCEIM        */
            },

            .qspiRefClkGating = /*!< Quad Spi Internal Reference Clock Gating. */
            {
                .enableQspiRefClk = false, /*!< Qspi reference clock gating    */
            },
            .tclkConfig = /*!< TCLK CLOCK configuration. */
            {
                .initialize = true, /*!< Initialize    */
                .tclkFreq[0] = 0U,  /*!< TCLK0         */
                .tclkFreq[1] = 0U,  /*!< TCLK1         */
                .tclkFreq[2] = 0U,  /*!< TCLK2         */
            },
            .traceClockConfig = /*!< Debug trace Clock Configuration. */
            {
                .initialize = true,                 /*!< Initialize    */
                .divEnable = true,                  /*!< TRACEDIVEN    */
                .source = CLOCK_TRACE_SRC_CORE_CLK, /*!< TRACECLK_SEL  */
                .divider = 0U,                      /*!< TRACEDIV      */
                .divFraction = false,               /*!< TRACEFRAC     */
            },
        },
    .pmcConfig =
        {
            .lpoClockConfig = /*!< Low Power Clock configuration.     */
            {
                .initialize = true, /*!< Initialize             */
                .enable = true,     /*!< Enable/disable LPO     */
                .trimValue = 0,     /*!< Trimming value for LPO */
            },
        },
};

/*! @brief Array of pointers to User configuration structures */
clock_manager_user_config_t const *g_clockManConfigsArr[] = {&clockMan1_InitConfig0};
/*! @brief Array of pointers to User defined Callbacks configuration structures */
clock_manager_callback_user_config_t *g_clockManCallbacksArr[] = {(void *)0};
/* END clockMan1. */

void BSPInit(void) {
    S32_SysTick->CSR = S32_SysTick_CSR_ENABLE(0u);
    S32_SysTick->RVR = S32_SysTick_RVR_RELOAD(SystemCoreClock / 1000u);
    // /* only initialize CVR on the first entry, to not cause time drift */
    S32_SysTick->CVR = S32_SysTick_CVR_CURRENT(0U);
    S32_SysTick->CSR =
        S32_SysTick_CSR_ENABLE(1u) | S32_SysTick_CSR_TICKINT(1u) | S32_SysTick_CSR_CLKSOURCE(1u);

    CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr,
                   CLOCK_MANAGER_CALLBACK_CNT);
    CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_FORCIBLE);

    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

    GPIOInit();
    FlexCANInit();
}

int BSPSetLED(uint8_t led, bool value) {
    if (led > 2) {
        return -1;
    }
    int led_shift = LED0;

    switch (led) {
    case 0:
        led_shift = LED0;
        break;
    case 1:
        led_shift = LED1;
        break;
    case 2:
        led_shift = LED2;
        break;
    default:
        return -1;
    }

    if (value) {
        PINS_DRV_SetPins(GPIO_PORT, 1 << led_shift);
    } else {
        PINS_DRV_ClearPins(GPIO_PORT, 1 << led_shift);
    }
    return 0;
}

uint32_t UDSMillis(void) { return OSIF_GetMilliseconds(); }
