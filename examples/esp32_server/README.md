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

This is tested on IDF 5.4.4, the last minor version before the new TWAI API:
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/migration-guides/release-5.x/5.5/peripherals.html

## Setup: Client

```sh
make && ./client
```

## Example Output: Server

```sh
idf.py monitor                               0
Executing action: monitor
Serial port /dev/ttyUSB0
Connecting....
Detecting chip type... ESP32-C3
Running idf_monitor in directory /home/njames/repos/iso14229/examples/esp32_server
Executing "/home/njames/.espressif/tools/python/v5.4.4/venv/bin/python /home/njames/.espressif/v5.4.4/esp-idf/tools/idf_monitor.py -p /dev/ttyUSB0 -b 115200 --toolchain-prefix riscv32-esp-elf- --target esp32c3 --revision 3 --decode-panic backtrace /home/njames/repos/iso14229/examples/esp32_server/build/esp32_server.elf -m '/home/njames/.espressif/tools/python/v5.4.4/venv/bin/python' '/home/njames/.espressif/v5.4.4/esp-idf/tools/idf.py'"...
--- esp-idf-monitor 1.9.0 on /dev/ttyUSB0 115200
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x1 (POWERON),boot:0xc (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd5820,len:0x158c
load:0x403cc710,len:0xc84
load:0x403ce710,len:0x2f64
entry 0x403cc71a
I (30) boot: ESP-IDF v5.4.4 2nd stage bootloader
I (30) boot: compile time Aug 17 2026 10:47:22
I (30) boot: chip revision: v0.3
I (30) boot: efuse block revision: v1.1
I (34) boot.esp32c3: SPI Speed      : 80MHz
I (38) boot.esp32c3: SPI Mode       : DIO
I (41) boot.esp32c3: SPI Flash Size : 2MB
I (45) boot: Enabling RNG early entropy source...
I (50) boot: Partition Table:
I (52) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (65) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (72) boot:  2 factory          factory app      00 00 00010000 00100000
I (78) boot: End of partition table
I (81) esp_image: segment 0: paddr=00010020 vaddr=3c020020 size=09490h ( 38032) map
I (95) esp_image: segment 1: paddr=000194b8 vaddr=3fc8c000 size=01728h (  5928) load
I (97) esp_image: segment 2: paddr=0001abe8 vaddr=40380000 size=05430h ( 21552) load
I (108) esp_image: segment 3: paddr=00020020 vaddr=42000020 size=1b0d4h (110804) map
I (129) esp_image: segment 4: paddr=0003b0fc vaddr=40385430 size=06a00h ( 27136) load
I (134) esp_image: segment 5: paddr=00041b04 vaddr=50000000 size=0001ch (    28) load
I (138) boot: Loaded app from partition at offset 0x10000
I (138) boot: Disabling RNG early entropy source...
I (154) cpu_start: Unicore app
I (162) cpu_start: GPIO 20 and 21 are used as console UART I/O pins
I (163) cpu_start: Pro cpu start user code
I (163) cpu_start: cpu freq: 160000000 Hz
I (165) app_init: Application information:
I (169) app_init: Project name:     esp32_server
I (173) app_init: App version:      0.9.0+e9f34ce-215-gb0e92b14-dir
I (179) app_init: Compile time:     Aug 17 2026 10:47:01
I (184) app_init: ELF file SHA256:  19950e373...
I (188) app_init: ESP-IDF:          v5.4.4
I (192) efuse_init: Min chip rev:     v0.3
I (196) efuse_init: Max chip rev:     v1.99
I (200) efuse_init: Chip rev:         v0.3
I (204) heap_init: Initializing. RAM available for dynamic allocation:
I (210) heap_init: At 3FC92710 len 0002D8F0 (182 KiB): RAM
I (215) heap_init: At 3FCC0000 len 0001C710 (113 KiB): Retention RAM
I (221) heap_init: At 3FCDC710 len 00002950 (10 KiB): Retention RAM
I (227) heap_init: At 5000001C len 00001FCC (7 KiB): RTCRAM
I (233) spi_flash: detected chip: generic
I (236) spi_flash: flash io: dio
I (239) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (246) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (252) main_task: Started on CPU0
I (282) main_task: Calling app_main()
I (282) gpio: GPIO[3]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (282) gpio: GPIO[4]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (282) gpio: GPIO[5]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (292) UDS: starting...
I (292) main_task: Returned from app_main()
I (31332) UDS: received event 10
I (31332) UDS: received 0x0001
I (31342) UDS: received event 10
I (31342) UDS: received 0x0001
I (31402) UDS: received event 10
I (31402) UDS: received 0x0001
I (31462) UDS: received event 10
I (31462) UDS: received 0x0001
I (31522) UDS: received event 10
I (31522) UDS: received 0x0001
I (31582) UDS: received event 10
I (31582) UDS: received 0x0001
I (31642) UDS: received event 10
I (31642) UDS: received 0x0001
I (31702) UDS: received event 10
I (31702) UDS: received 0x0001
I (31762) UDS: received event 10
I (31762) UDS: received 0x0001
I (31822) UDS: received event 10
I (31822) UDS: received 0x0001
I (31882) UDS: received event 10
I (31882) UDS: received 0x0001
I (31942) UDS: received event 10
I (31942) UDS: received 0x0001
I (32002) UDS: received event 10
I (32002) UDS: received 0x0001
I (32062) UDS: received event 10
I (32062) UDS: received 0x0001
I (32122) UDS: received event 10
I (32122) UDS: received 0x0001
I (32182) UDS: received event 10
I (32182) UDS: received 0x0001
I (32242) UDS: received event 10
I (32242) UDS: received 0x0001
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