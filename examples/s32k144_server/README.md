# NXP S32K144 Server Example

UDS server example for NXP S32K144 microcontroller.

## Overview

This example demonstrates a UDS server running on the NXP S32K144 evaluation board.

## Files

See the source files in the `examples/s32k144_server/` directory.

## Hardware

This demo uses the [S32K144-EVB](https://www.keil.arm.com/boards/nxp-s32k144-evb-rev-a-ee981eb/features/)


# Building 

This uses `gcc-arm-none-eabi`, make sure it's installed first.  

```sh
sudo apt install gcc-arm-none-eabi
bazel build --config=s32k //examples/s32k144:main
```

# CMSIS-DAP

1. update programming interface from "OpenSDA" to CMSIS-DAP [1](https://community.nxp.com/t5/S32K/S32K144evb-with-CMSIS-DAP/td-p/1227900) [2](https://developer.arm.com/documentation/kan299/latest/)
2. install `pyocd`
3. `pyocd pack install S32K144UAxxxLLx`
4. update `/etc/udev/rules.d/50-cmsis-dap.rules` with the following:

```
# c251:f002 S32K144-EVB
SUBSYSTEM=="usb", ATTR{idVendor}=="c251", ATTR{idProduct}=="f002", MODE:="666"
```

```
pyocd gdbserver --persist -Otarget_override=S32K144UAxxxLLx
```

# Physical Setup

- The S32K144-EVB needs an external 12V supply to power the CAN transciever

