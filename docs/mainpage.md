# iso14229 {#mainpage}

iso14229 is a UDS (ISO14229) library for writing servers and clients.

**Source Code:** https://github.com/driftregion/iso14229

## Quick Start {#quickstart}

1. Download the sources `iso14229.c` and `iso14229.h` from the [releases page](https://github.com/driftregion/iso14229/releases) and add them to your project.
2. Use the \ref examples_sec to guide your implementation.

---

## Examples {#examples_sec}

To access the examples, clone or download the repository from https://github.com/driftregion/iso14229

| Example | Description |
|---------|-------------|
| \ref examples/linux_server/README.md "linux_server" | Basic Linux server using socketcan ISO-TP |
| \ref examples/linux_rdbi_wdbi/README.md "linux_rdbi_wdbi" | Read/Write Data By Identifier (0x22/0x2E) |
| \ref examples/linux_server_0x27/README.md "linux_server_0x27" | Security Access (0x27) |
| \ref examples/arduino_server/README.md "arduino_server" | Arduino server |
| \ref examples/esp32_server/README.md "esp32_server" | ESP32 server |
| \ref examples/s32k144_server/README.md "s32k144_server" | NXP S32K144 server |
| \ref examples/doip_client/README.md "doip_client" | DoIP client |

---

## API Documentation

- \ref server "Server API"
- \ref client "Client API"
- \ref services "UDS Services"

## Configuration {#configuration}

Configure the library at compilation time with preprocessor defines:

### Transport Selection {#transport_layers}

| Transport | Define | Description | Suitable For Targets | Example Implementations |
|-----------|--------|-------------|-------------|------------|
| **isotp_sock** | `-DUDS_TP_ISOTP_SOCK` | Linux kernel ISO-TP socket | Linux newer than 5.10  |  \ref examples/linux_server_0x27/README.md "linux_server_0x27" |
| **isotp_c_socketcan** | `-DUDS_TP_ISOTP_C_SOCKETCAN` | isotp-c over SocketCAN | Linux newer than 2.6.25 | \ref examples/linux_server_0x27/README.md "linux_server_0x27" |
| **isotp_c** | `-DUDS_TP_ISOTP_C` | Software ISO-TP | Everything else | \ref examples/arduino_server/README.md "arduino_server" \ref examples/esp32_server/README.md "esp32_server" \ref examples/s32k144_server/README.md "s32k144_server" |
| **isotp_mock** | `-DUDS_TP_ISOTP_MOCK` | In-memory transport for testing | platform-independent unit tests | see unit tests |
| **doip** | `-DUDS_TP_DOIP` | DoIP (iso 13400) client | platform-independent unit tests | \ref examples/linux_rdbi_wdbi/README.md |

### System Selection Override

The system is usually detected by default, but can be overridden with the following options:

| Define | Values |
|--------|--------|
| `-DUDS_SYS=` | `UDS_SYS_UNIX`, `UDS_SYS_WINDOWS`, `UDS_SYS_ARDUINO`, `UDS_SYS_ESP32`, `UDS_SYS_CUSTOM` |

For examples of `UDS_SYS_CUSTOM`, see \ref examples/arduino_server/README.md "arduino_server", \ref examples/esp32_server/README.md "esp32_server", \ref examples/s32k144_server/README.md "s32k144_server".

### Logging

Logging is disabled by default. However, it is very helpful to identify problems during initial server/client bringup.

| Define | Values |
|--------|--------|
| `-DUDS_LOG_LEVEL=` | `UDS_LOG_NONE`, `UDS_LOG_ERROR`, `UDS_LOG_WARN`, `UDS_LOG_INFO`, `UDS_LOG_DEBUG`, `UDS_LOG_VERBOSE` |

### Other Options

- `-DUDS_SERVER_...` - Server configuration options (see \ref server_configuration)
- `-DUDS_CLIENT_...` - Client configuration options (see \ref client_configuration)

---
