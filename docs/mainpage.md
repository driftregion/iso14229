# iso14229 - UDS Server / Client Library Documentation {#mainpage}

## Quick Start {#quickstart}

1. Download the sources `iso14229.c` and `iso14229.h` from the [releases page](https://github.com/driftregion/iso14229/releases) and add them to your project.
2. See the quickstart example in the [Server](docs/server.md) or [Client](docs/client.md) documentation. 

## Index

- [Server](docs/server.md) 
- [Client](docs/client.md)
- [Compile-Time Configuration Options](docs/config.md)
- [Porting Guide](docs/porting_guide.md)

## Examples {#examples_sec}

| Example | Description |
|---------|-------------|
| [`linux_rdbi_wdbi`](examples/linux_rdbi_wdbi/README.md) | UDS Server on Linux implementing Read/Write Data By Identifier (0x22/0x2E) |
| [`linux_security_access`](examples/linux_security_access/README.md) | UDS Server on Linux implementing Security Access (0x27) |
| [`arduino_server`](examples/arduino_server/README.md) | UDS Server on Arduino MKR-WIFI 1010 with MKR CAN Shield |
|  [`esp32_server`](examples/esp32_server/README.md ) | UDS Server on ESP32-C3-32S with Waveshare SN65HVD230 CAN Transciever |
|  [`s32k144_server`](examples/s32k144_server/README.md) | UDS Server on NXP S32K144 eval board with Waveshare SN65HVD230 CAN Transciever | 
|  [`stm32g474`](examples/stm32g474/README.md) | UDS Server on STM NUCLEO-G474RE eval board with Waveshare SN65HVD230 CAN Transciever | 

## Supported Services {#supported-services}

| Service ID | Service Name | Server | Client | 
|------------|--------------|--------|--------|
| 0x10 | Diagnostic Session Control | Y | Y |
| 0x11 | ECU Reset | Y | Y | 
| 0x14 | Clear Diagnostic Information | Y | N |
| 0x19 | Read DTC Information | N | N |
| 0x22 | Read Data By Identifier | Y | Y |
| 0x23 | Read Memory By Address | N | N | 
| 0x24 | Read Scaling Data By Identifier | N | N | 
| 0x27 | Security Access | Y | Y | 
| 0x28 | Communication Control | Y | Y | 
| 0x2A | Read Periodic Data By Identifier | N | N | 
| 0x2C | Dynamically Define Data Identifier | N | N | 
| 0x2E | Write Data By Identifier | Y | Y | 
| 0x2F | Input/Output Control By Identifier | Y | N | 
| 0x31 | Routine Control | Y | Y |
| 0x34 | Request Download | Y | Y |
| 0x35 | Request Upload | Y | Y | 
| 0x36 | Transfer Data | Y | Y | 
| 0x37 | Request Transfer Exit | Y | Y | 
| 0x38 | Request File Transfer | Y | Y | 
| 0x3D | Write Memory By Address | Y | N | 
| 0x3E | Tester Present | Y | Y | 
| 0x83 | Access Timing Parameter | N | N | 
| 0x84 | Secured Data Transmission | N | N | 
| 0x85 | Control DTC Setting | Y | Y | 
| 0x86 | Response On Event | N | N | 
| 0x87 | Link Control | Y | N | 