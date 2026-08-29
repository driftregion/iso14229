# Porting Guide

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
UDS_CUSTOM_MILLIS
```

## Logging

Logging is disabled by default with `-DUDS_LOG_LEVEL=UDS_LOG_NONE`. 
However, logging can help to quickly identify problems during initial server/client bringup.
To use logging, your target system must have `printf` support.

## Integrating 

Now that iso14229 compiles, it can be integrated with the target system. 

### Hooks

iso14229 calls into the target system via hooks.
New targets will need to implement the following hooks:

- `uint32_t UDSMillis(void)`
- `int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size)`

See the documentation for each function, and the example here: \ref examples/s32k144_server/README.md "s32k144_server".

### Polling
The polling functions `UDSServerPoll()` and `UDSClientPoll()` need to be called regularly at intervals of 5 ms or less. 

### Thread Safety

The iso14229 API is not designed to be called from multiple thread or interrupt contexts.

### Message Queues

On bare-metal targets where CAN frames are handled in ISR context, 

