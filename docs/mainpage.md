# iso14229 {#mainpage}

A portable, embeddable ISO14229-1 (UDS) library for writing UDS clients and servers.

**Source Code:** https://github.com/driftregion/iso14229

## Quick Start {#quickstart}

1. Download the sources and add them to your project:
    - [iso14229.h](https://github.com/driftregion/iso14229/raw/main/iso14229.h)
    - [iso14229.c](https://github.com/driftregion/iso14229/raw/main/iso14229.c)
2. Use the \ref examples_sec to guide your implementation.

---

## Table of Contents

- \ref server "Server API"
- \ref client "Client API"
- \ref services "UDS Services"
- \ref examples_sec
- \ref transport_layers
- \ref configuration

---

## Examples {#examples_sec}

| Example | Description |
|---------|-------------|
| \ref examples/linux_server/README.md "linux_server" | Basic Linux server using socketcan ISO-TP |
| \ref examples/linux_rdbi_wdbi/README.md "linux_rdbi_wdbi" | Read/Write Data By Identifier (0x22/0x2E) |
| \ref examples/linux_server_0x27/README.md "linux_server_0x27" | Security Access (0x27) |
| \ref examples/arduino_server/README.md "arduino_server" | Arduino server |
| \ref examples/esp32_server/README.md "esp32_server" | ESP32 server |
| \ref examples/s32k144_server/README.md "s32k144_server" | NXP S32K144 server |

---

## Transport Layers {#transport_layers}

| Transport | Define | Description |
|-----------|--------|-------------|
| **isotp_sock** | `-DUDS_TP_ISOTP_SOCK` | Linux kernel ISO-TP socket (recommended for Linux) |
| **isotp_c_socketcan** | `-DUDS_TP_ISOTP_C_SOCKETCAN` | isotp-c over SocketCAN (Linux) |
| **isotp_c** | `-DUDS_TP_ISOTP_C` | Software ISO-TP (embedded, Windows, etc.) |
| **isotp_mock** | `-DUDS_TP_ISOTP_MOCK` | In-memory transport for testing |

---

## Configuration {#configuration}

Configure the library at compilation time with preprocessor defines:

### Transport Selection

| Define | Description |
|--------|-------------|
| `-DUDS_TP_ISOTP_SOCK` | build the Linux kernel ISO-TP socket |
| `-DUDS_TP_ISOTP_C` | build the isotp-c software implementation |
| `-DUDS_TP_ISOTP_C_SOCKETCAN` | build isotp-c over SocketCAN |
| `-DUDS_TP_ISOTP_MOCK` | build the mock transport for testing |

### System Selection Override

The system is usually detected by default, but can be overridden with the following options:

| Define | Values |
|--------|--------|
| `-DUDS_SYS=` | `UDS_SYS_UNIX`, `UDS_SYS_WINDOWS`, `UDS_SYS_ARDUINO`, `UDS_SYS_ESP32`, `UDS_SYS_CUSTOM` |

### Logging

Logging is disabled by default. However, it is very helpful to identify problems during initial server/client bringup.

| Define | Values |
|--------|--------|
| `-DUDS_LOG_LEVEL=` | `UDS_LOG_NONE`, `UDS_LOG_ERROR`, `UDS_LOG_WARN`, `UDS_LOG_INFO`, `UDS_LOG_DEBUG`, `UDS_LOG_VERBOSE` |

### Other Options

- `-DUDS_SERVER_...` - Server configuration options (see \ref server_configuration)
- `-DUDS_CLIENT_...` - Client configuration options (see \ref client_configuration)

---
