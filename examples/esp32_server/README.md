# ESP32 Server Example

UDS server example for ESP32 platform.

## Overview

This example demonstrates a UDS server running on an ESP32 with TWAI (CAN) transceiver support.

Also included is an example client which can be run from a linux computer.

## Required Hardware

server:
- [ESP32-C3-32S](https://docs.ai-thinker.com/_media/esp32/docs/esp-c3-32s-kit-v1.0_specification.pdf)
- [Waveshare SN65HVD230 CAN Board](https://www.waveshare.com/sn65hvd230-can-board.htm)

client:
- a socketcan-compatible adapter

## Setup: Server

1. download and install `esp-idf`
2. connect CAN board to ESP32

```sh
. ~/esp/esp-idf/export.sh
idf.py set-target esp32c3
idf.py build flash monitor
```

## Setup: Client

```sh
make && ./client
```

## Example Output: Server

```sh
idf.py monitor
Executing action: monitor
Serial port /dev/ttyUSB0
Connecting....
Detecting chip type... ESP32-C3
Running idf_monitor in directory /home/user/repos/iso14229/examples/esp32_server
Executing "/home/user/.espressif/python_env/idf5.0_py3.10_env/bin/python /home/user/esp/esp-idf/tools/idf_monitor.py -p /dev/ttyUSB0 -b 115200 --toolchain-prefix riscv32-esp-elf- --target esp32c3 --decode-panic backtrace /home/user/repos/iso14229/examples/esp32_server/build/esp32_server.elf -m '/home/user/.espressif/python_env/idf5.0_py3.10_env/bin/python' '/home/user/esp/esp-idf/tools/idf.py'"...
--- idf_monitor on /dev/ttyUSB0 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x1 (POWERON),boot:0xc (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd6110,len:0x16ac
load:0x403ce000,len:0x95c
load:0x403cf600,len:0x2df8
entry 0x403ce000
I (30) boot: ESP-IDF v5.0-beta1-dirty 2nd stage bootloader
I (30) boot: compile time 20:41:53
I (30) boot: chip revision: V003
I (33) boot.esp32c3: SPI Speed      : 80MHz
I (38) boot.esp32c3: SPI Mode       : DIO
I (43) boot.esp32c3: SPI Flash Size : 2MB
I (47) boot: Enabling RNG early entropy source...
I (53) boot: Partition Table:
I (56) boot: ## Label            Usage          Type ST Offset   Length
I (64) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (71) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (78) boot:  2 factory          factory app      00 00 00010000 00100000
I (86) boot: End of partition table
I (90) esp_image: segment 0: paddr=00010020 vaddr=3c020020 size=07e50h ( 32336) map
I (104) esp_image: segment 1: paddr=00017e78 vaddr=3fc8aa00 size=01524h (  5412) load
I (108) esp_image: segment 2: paddr=000193a4 vaddr=40380000 size=06c74h ( 27764) load
I (121) esp_image: segment 3: paddr=00020020 vaddr=42000020 size=1b740h (112448) map
I (142) esp_image: segment 4: paddr=0003b768 vaddr=40386c74 size=03d68h ( 15720) load
I (145) esp_image: segment 5: paddr=0003f4d8 vaddr=50000010 size=00010h (    16) load
I (151) boot: Loaded app from partition at offset 0x10000
I (154) boot: Disabling RNG early entropy source...
I (170) cpu_start: Pro cpu up.
I (179) cpu_start: Pro cpu start user code
I (180) cpu_start: cpu freq: 160000000 Hz
I (180) cpu_start: Application information:
I (183) cpu_start: Project name:     esp32_server
I (188) cpu_start: App version:      0.9.0+e9f34ce-199-g99bcb7a0-dir
I (195) cpu_start: Compile time:     Oct  5 2025 20:41:49
I (201) cpu_start: ELF file SHA256:  dbd9f740fc5bce11...
I (207) cpu_start: ESP-IDF:          v5.0-beta1-dirty
I (213) heap_init: Initializing. RAM available for dynamic allocation:
I (220) heap_init: At 3FC90F00 len 0002F100 (188 KiB): DRAM
I (226) heap_init: At 3FCC0000 len 0001F060 (124 KiB): STACK/DRAM
I (233) heap_init: At 50000020 len 00001FE0 (7 KiB): RTCRAM
I (240) spi_flash: detected chip: generic
I (244) spi_flash: flash io: dio
I (249) cpu_start: Starting scheduler.
I (253) gpio: GPIO[3]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (253) gpio: GPIO[4]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (263) gpio: GPIO[5]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (273) UDS: starting...
I (22833) UDS: received event 10
I (22833) UDS: received 0x0001
I (22843) UDS: received event 10
I (22843) UDS: received 0x0001
I (22903) UDS: received event 10
I (22903) UDS: received 0x0001
# ...
```

## Example Output: Client

```sh
make && ./client
cc -DUDS_TP_ISOTP_SOCK -DUDS_LINES -DUDS_LOG_LEVEL=UDS_LOG_INFO -g -Imain  main/iso14229.c client.c -o client
I (3081693397) src/tp/isotp_sock.c: configuring fd: 4 as functional
I (3081693397) src/tp/isotp_sock.c: client initialized phys link (fd 3) rx 0x7e8 tx 0x7e0 func link (fd 4) rx 0x7e8 tx 0x7df
I (3081693397) client.c: polling
I (3081693397) src/client.c: client state: Idle (0) -> Sending (1)
I (3081693397) src/client.c: client state: Sending (1) -> AwaitSendComplete (2)
I (3081693397) client.c: UDS_EVT_SendComplete (26)
I (3081693397) src/client.c: client state: AwaitSendComplete (2) -> AwaitResponse (3)
I (3081693405) src/client.c: received 3 bytes. Processing...
I (3081693405) client.c: UDS_EVT_ResponseReceived (27)

# === truncated ===

I (3081694365) src/client.c: received 3 bytes. Processing...
I (3081694365) client.c: UDS_EVT_ResponseReceived (27)
I (3081694365) client.c: WDBI response received
I (3081694365) src/client.c: client state: AwaitResponse (3) -> Idle (0)
I (3081694365) client.c: UDS_EVT_Idle (28)
```

## Example Output: candump

```sh
candump can0
  can0  7E8   [4]  03 6E 00 01
  can0  7E0   [5]  04 2E 00 01 01
  can0  7E8   [4]  03 6E 00 01
  can0  7E0   [5]  04 2E 00 01 02
  can0  7E8   [4]  03 6E 00 01
  can0  7E0   [5]  04 2E 00 01 03
  can0  7E8   [4]  03 6E 00 01
  can0  7E0   [5]  04 2E 00 01 04
  can0  7E8   [4]  03 6E 00 01
  can0  7E0   [5]  04 2E 00 01 05
  can0  7E8   [4]  03 6E 00 01
# ...
```