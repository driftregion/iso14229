# Porting Guide {#porting-guide}

iso14229 can be configured to run on most targets without modifying the source files `iso14229.c/.h`.
This guide describes porting: setting up iso14229 to run on a new target for the first time.
In order to use this guide, you must have a C toolchain for your target, as well as an existing project which compiles successfully.

> [!NOTE]
> If your target uses linux, Windows, Arduino, or esp-idf, consider consulting the [examples](docs/mainpage.md#examples_sec) first. This guide mainly considers embedded targets without existing support.

If you encounter a problem while using this guide, please open an issue.

## Target Requirements

The target must have a CAN interface, consisting of a CAN controller and a transciever.
The controller handles the CAN protocol details including arbitration, error handling, and most importantly: serializing and deserializing data. 
The transciever handles level-shifting (CAN uses a differential-pair) and sometimes optical isolation.

Microcontrollers often have an on-chip memory-mapped CAN controller peripheral. 
Stand-alone CAN controllers such as the SPI-controllable MCP2515 are also available, often used when the target does not have an on-chip CAN controller (for example, Raspberry Pi CAN hats), or when the target needs additional CAN controllers for multiple buses.

The transciever (also called PHY, for physical layer) is always a separate integrated circuit like the TJA1050.

## Host Requirements

The host (development) machine must also have a CAN interface, used to test the target.
On Linux this may be a socketcan interface paired with software such as cantools (which provides the `cansend` utility), or SaavyCAN.
On Windows or MacOS this will be a proprietary CAN interface made by PEAK-CAN, Kvaser, ZLG or the like, paired with some GUI software.


# Compiling the Library

Move the source files `iso14229.c/.h` into your project directory and confirm that they compile.
Set the following preprocessor directives in your build system or command line:
```txt
UDS_SYS=UDS_SYS_CUSTOM
UDS_TP_ISOTP_C
```

If your target has `printf` support, then you can enable logging which will help during bringup. 
```txt
DUDS_LOG_LEVEL=UDS_LOG_VERBOSE
```

iso14229 calls into the target system via hooks.
New targets will need to implement the following hooks:

1. `UDSMillis`. Example for STM32:
```c
uint32_t UDSMillis(void) { return HAL_GetTick(); }
```


2. `isotp_user_send_can`. Example for STM32:
```c
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
                        void *user_data) {
    (void)user_data;
    FDCAN_TxHeaderTypeDef TxHeader = {0};
    TxHeader.Identifier = arbitration_id;
    TxHeader.DataLength = size;
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, (uint8_t *)data) != HAL_OK) {
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}
```
See also: isotp-c documentation.

### Polling
The polling functions `UDSServerPoll()` and `UDSClientPoll()` need to be called regularly at intervals of 5 ms or less. 

> [!NOTE]
> **Thread Safety**
> The iso14229 API is not designed to be called from separate threads or interrupt contexts. 
Either keep everything in the main context or in a dedicated RTOS task/thread.

### Example Ports

- [`esp32_server`](examples/esp32_server/README.md )
- [`s32k144_server`](examples/s32k144_server/README.md) 
- [`stm32g474`](examples/stm32g474/README.md) 

