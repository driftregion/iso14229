# Arduino Server Example

UDS server example for Arduino platforms.

## Overview

This example demonstrates a UDS server running on an Arduino with CAN shield support.

## Files

See the source files in the `examples/arduino_server/` directory.

## Required Hardware

- [Arduino MKR-WIFI 1010](https://store-usa.arduino.cc/products/arduino-mkr-wifi-1010)
- [MKR CAN Shield](https://store.arduino.cc/products/arduino-mkr-can-shield)

On my MKR CAN Shield, the labeling of CAN-H and CAN-L on silkscreen on the MKR CAN Shield was contradictory on the top and bottom of the board. The top is correct.

## Running

```
# flash the program
arduino-cli -b arduino:samd:mkrwifi1010 compile examples/arduino_server/main -u -p /dev/ttyACM0

# install requirements
pip install -r requirements.txt
```