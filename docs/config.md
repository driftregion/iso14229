# Compile-Time Configuration {#config}

This page lists the public compile-time configuration API

| Define | Default | Valid Range | Description |
|--------|---------|-------------|-------------|
| \ref UDS_SYS | auto-detected | \ref uds_sys_ | target system |
| UDS_CUSTOM_MILLIS | unset | set or unset | if unset (default), iso14229 provides UDSMillis() for the detected @ref UDS_SYS platform. If set, the library user must provide their own UDSMillis() implementation. |
| UDS_TP_ISOTP_SOCK | unset | set or unset | builds transport: Linux kernel ISO-TP socket. Suitable for linux newer than 5.10. See \ref UDSServerTpIsoTpSockInit and \ref UDSClientTpIsoTpSockInit. |
| UDS_TP_ISOTP_C_SOCKETCAN | unset | set or unset | builds transport: isotp-c over SocketCAN. Suitable for Linux newer than 2.6.25. See \ref UDSTpISOTpCSocketCANInit. |
| UDS_TP_ISOTP_C | unset* | set or unset | builds transport: isotp-c. Suitable for *everything*, but you must bring your own CAN interface. See \ref UDSServerTpISOTpCInit, \ref UDSClientTpISOTpCInit and the \ref porting-guide. |
| \ref UDS_LOG_LEVEL | \ref UDS_LOG_NONE | \ref uds_log_level_ | iso14229 internal log level. Set UDS_LOG_LEVEL=UDS_LOG_DEBUG for a pleasant first-time bringup experience, then turn it off when you're finished. |
| \ref UDS_SERVER_DEFAULT_P2_MS | 50 | - | Default P2 timeout (ms) |
| \ref UDS_SERVER_DEFAULT_P2_STAR_MS | 5000 | - | Default P2* timeout (ms) |
| `UDS_SERVER_DEFAULT_S3_MS` | 5100 | - | Session timeout (ms) |
| `UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS` | 60 | - | Delay before ECU reset (ms) |
| `UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS` | 1000 | - | Boot delay for security access (ms) |
| `UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS` | 1000 | - | Delay after auth failure (ms) |
| `UDS_SERVER_SEND_BUF_SIZE` | 4095 | - | Send buffer size |
| `UDS_SERVER_RECV_BUF_SIZE` | 4095 | - |  Receive buffer size |
| `UDS_CLIENT_DEFAULT_P2_MS` | 150 | - | Default P2 timeout (ms) |
| `UDS_CLIENT_DEFAULT_P2_STAR_MS` | 1500  | - | Default P2* timeout (ms) |
| `UDS_CLIENT_SEND_BUF_SIZE` | 4095 | - | Send buffer size |
| `UDS_CLIENT_RECV_BUF_SIZE` | 4095 | - | Receive buffer size |

- *except on `UDS_SYS=UDS_SYS_ARDUINO` and `UDS_SYS=UDS_SYS_ESP32`, where `UDS_TP_ISOTP_C` is set by default.
