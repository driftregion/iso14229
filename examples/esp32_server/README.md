# ESP32 Server Example

UDS server example for ESP32 platforms.

## Overview

This example demonstrates a UDS server running on an ESP32 with TWAI (CAN) transceiver support.

## Files

See the source files in the `examples/esp32_server/` directory.

## Required Hardware

- [ESP32-C3-32S](https://docs.ai-thinker.com/_media/esp32/docs/esp-c3-32s-kit-v1.0_specification.pdf)
- [Waveshare SN65HVD230 CAN Board](https://www.waveshare.com/sn65hvd230-can-board.htm)

## Setup

1. download and install `esp-idf`
2. connect CAN board to ESP32

```sh
. ~/esp/esp-idf/export.sh
idf.py set-target esp32c3
idf.py build flash monitor
```
