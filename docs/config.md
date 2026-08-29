This page documents compile-time configuration options of iso14229.

### System Selection 

The system is detected by default for supported platforms, and can be overridden with the following options:

| Define | Values |
|--------|--------|
| `-DUDS_SYS=` | `UDS_SYS_UNIX`, `UDS_SYS_WINDOWS`, `UDS_SYS_ARDUINO`, `UDS_SYS_ESP32`, `UDS_SYS_CUSTOM` |

This guide is oriented towards unsupported systems, which use `UDS_SYS_CUSTOM`.
See an example here: \ref examples/s32k144_server/README.md "s32k144_server".

### Transport Selection {#transport_layers}

Embedded targets will use the `isotp_c` transport layer, enabled with `-DUDS_TP_ISOTP_C`.

| Transport | Define | Description | Suitable For Targets | Example Implementations |
|-----------|--------|-------------|-------------|------------|
| **isotp_sock** | `-DUDS_TP_ISOTP_SOCK` | Linux kernel ISO-TP socket | Linux newer than 5.10  |  \ref examples/linux_server_0x27/README.md "linux_server_0x27" |
| **isotp_c_socketcan** | `-DUDS_TP_ISOTP_C_SOCKETCAN` | isotp-c over SocketCAN | Linux newer than 2.6.25 | \ref examples/linux_server_0x27/README.md "linux_server_0x27" |
| **isotp_c** | `-DUDS_TP_ISOTP_C` | Software ISO-TP | Everything else | \ref examples/arduino_server/README.md "arduino_server" \ref examples/esp32_server/README.md "esp32_server" \ref examples/s32k144_server/README.md "s32k144_server" |
| **isotp_mock** | `-DUDS_TP_ISOTP_MOCK` | In-memory transport for testing | platform-independent unit tests | see unit tests |

### Logging
| Define | Values |
|--------|--------|
| `-DUDS_LOG_LEVEL=` | `UDS_LOG_NONE`, `UDS_LOG_ERROR`, `UDS_LOG_WARN`, `UDS_LOG_INFO`, `UDS_LOG_DEBUG`, `UDS_LOG_VERBOSE` |


### Server Configuration 

- `-DUDS_SERVER_...` - Server configuration options (see \ref server_configuration)

### Client Configuration

- `-DUDS_CLIENT_...` - Client configuration options (see \ref client_configuration)
