# iso14229

<p align="center">
<a href="https://github.com/driftregion/iso14229/actions"><img src="https://github.com/driftregion/iso14229/actions/workflows/runtests.yml/badge.svg" alt="Build Status"></a>
<a href="./LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
</p>

iso14229 is an implementation of UDS (ISO14229) targeting embedded systems. It is tested with [`isotp-c`](https://github.com/lishen2/isotp-c) as well as [linux kernel](https://github.com/linux-can/can-utils/blob/master/include/linux/can/isotp.h) ISO15765-2 (ISO-TP) transport layer implementations. 

API status: Major version zero (0.y.z) **(not yet stable)**. Anything MAY change at any time.

## Using this library 

1. Download `iso14229.zip` from the [releases page](https://github.com/driftregion/iso14229/releases), copy `iso14229.c` and `iso14229.h` into your project.
2. Select a transport layer using the table below and enable it by defining the `copt` in your project's build configuration.

| Transport Layer | `copt` | Suitable for | 
| - | - | - |
| ISO-TP sockets | `-DUDS_TP_ISOTP_SOCK` | Linux socketcan |
| isotp-c on socketcan | `-DUDS_TP_ISOTP_C_SOCKETCAN` | Linux socketcan |
| isotp-c (bring your own CAN) | `-DUDS_TP_ISOTP_C` | embedded systems, Windows, Linux non-socketcan, ... |

3. Refer to the [examples](./examples) and [tests](./test) for usage.


## Compile-Time Features

The following features are configured with preprocessor defines:

| Define | Description | Valid values | 
| - | - | - |
| `-DUDS_TP_ISOTP_C` | build the isotp-c transport layer (recommended for bare-metal systems) | defined or not |
| `-DUDS_TP_ISOTP_SOCK` | build the isotp socket transport layer (recommended for linux)  | defined or not |
| `-DUDS_TP_ISOTP_C_SOCKETCAN` | build the isotp-c transport layer with socketcan support (linux-only)  | defined or not |
| `-DUDS_LOG_LEVEL=...`| Sets the logging level. Internal log messages are useful for bringup and unit tests. This defaults to `UDS_LOG_LEVEL=UDS_LOG_NONE` which completely disables logging, ensuring that no logging-related code goes in to the binary. | `UDS_LOG_NONE, UDS_LOG_ERROR, UDS_LOG_WARN, UDS_LOG_INFO, UDS_LOG_DEBUG, UDS_LOG_VERBOSE` |
| `-DUDS_SERVER_...` | server configuration options | see [`src/config.h`](src/config.h) |
| `-DUDS_CLIENT_...` | client configuration options | see [`src/config.h`](src/config.h) |
| `-DUDS_SYS=` | Selects target system. See [`src/sys.h`](src/sys.h) | `UDS_SYS_CUSTOM,UDS_SYS_UNIX,UDS_SYS_WINDOWS,UDS_SYS_ARDUINO,UDS_SYS_ESP32`  |

### Deprecated Compile-Time Features

| Define | Reason for deprecation | mitigation | 
| `-DUDS_ENABLE_ASSERT` | redundant | Use the standard `-DNDEBUG` to disable assertions. |
| `-DUDS_ENABLE_DEBUG_PRINT` | replaced by `UDS_LOG` | Use `-DUDS_LOG_LEVEL=` to set or disable logging. |

## Features

- entirely static memory allocation. (no `malloc`, `calloc`, ...)
- highly portable and tested
    - architectures: arm, x86-64, ppc, ppc64, risc
    - systems: linux, Windows, esp32, Arduino, NXP s32k
    - transports: isotp-c, linux isotp sockets

##  supported functions (server and client )

| SID | name | supported |
| - | - | - |
| 0x10 | diagnostic session control | ✅ |
| 0x11 | ECU reset | ✅ |
| 0x14 | clear diagnostic information | ❌ |
| 0x19 | read DTC information | ❌ |
| 0x22 | read data by identifier | ✅ |
| 0x23 | read memory by address | ❌ |
| 0x24 | read scaling data by identifier | ❌ |
| 0x27 | security access | ✅ |
| 0x28 | communication control | ✅ |
| 0x2A | read periodic data by identifier | ❌ |
| 0x2C | dynamically define data identifier | ❌ |
| 0x2E | write data by identifier | ✅ |
| 0x2F | input control by identifier | ❌ |
| 0x31 | routine control | ✅ |
| 0x34 | request download | ✅ |
| 0x35 | request upload | ✅ |
| 0x36 | transfer data | ✅ |
| 0x37 | request transfer exit | ✅ |
| 0x38 | request file transfer | ✅ |
| 0x3D | write memory by address | ❌ |
| 0x3E | tester present | ✅ |
| 0x83 | access timing parameter | ❌ |
| 0x84 | secured data transmission | ❌ |
| 0x85 | control DTC setting | ✅ |
| 0x86 | response on event | ❌ |

# Documentation: Server

## Server Events

see `enum UDSEvent` in [src/uds.h](src/uds.h)

### `UDS_EVT_DiagSessCtrl` (0x10)

#### Arguments

```c
typedef struct {
    const enum UDSDiagnosticSessionType type; /**< requested session type */
    uint16_t p2_ms;                           /**< optional return value: p2 timing override */
    uint32_t p2_star_ms;                      /**< optional return value: p2* timing override */
} UDSDiagSessCtrlArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request to change diagnostic level accepted. |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | The server doesn't support this diagnostic level |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | The server can't/won't transition to the specified diagnostic level at this time |

### `UDS_EVT_ECUReset` (0x11)

#### Arguments

```c
typedef struct {
    const enum UDSECUResetType type; /**< reset type requested by client */
    uint8_t powerDownTime; /**< Optional response: notify client of time until shutdown (0-254) 255
                              indicates that a time is not available. */
} UDSECUResetArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request to reset ECU accepted.  |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | The server doesn't support the specified type of ECU reset |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | The server can't reset now |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | The current level of security access doesn't permit this type of ECU reset |

### `UDS_EVT_ReadDataByIdent` (0x22)

#### Arguments

```c
typedef struct {
    const uint16_t dataId; /*! data identifier */
    /*! function for copying to the server send buffer. Returns `UDS_PositiveResponse` on success and `UDS_NRC_ResponseTooLong` if the length of the data to be copied exceeds that of the server send buffer */
    const uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); 
} UDSRDBIArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request to read data accepted (be sure to call `copy(...)`) |
| `0x14` | `UDS_NRC_ResponseTooLong` | The total length of the response message exceeds the transport buffer size |
| `0x31` | `UDS_NRC_RequestOutOfRange` | The requested data identifer isn't supported |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | The current level of security access doesn't permit reading the requested data identifier |

### `UDS_EVT_SecAccessRequestSeed`, `UDS_EVT_SecAccessValidateKey` (0x27)

#### Arguments

```c
typedef struct {
    const uint8_t level;             /*! requested security level */
    const uint8_t *const dataRecord; /*! pointer to request data */
    const uint16_t len;              /*! size of request data */
    /*! function for copying to the server send buffer. Returns `UDS_PositiveResponse` on success and `UDS_NRC_ResponseTooLong` if the length of the data to be copied exceeds that of the server send buffer */
    uint8_t (*copySeed)(UDSServer_t *srv, const void *src,
                        uint16_t len);
} UDSSecAccessRequestSeedArgs_t;

typedef struct {
    const uint8_t level;      /*! security level to be validated */
    const uint8_t *const key; /*! key sent by client */
    const uint16_t len;       /*! length of key */
} UDSSecAccessValidateKeyArgs_t;
```
#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request accepted  |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | The requested security level is not supported |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | The server can't handle the request right now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | The `dataRecord` contains invalid data |
| `0x35` | `UDS_NRC_InvalidKey` | The key doesn't match |
| `0x36` | `UDS_NRC_ExceededNumberOfAttempts` | False attempt limit reached |
| `0x37` | `UDS_NRC_RequiredTimeDelayNotExpired` | RequestSeed request received and delay timer is still active |

### `UDS_EVT_CommCtrl` (0x28)

#### Arguments

```c
typedef struct {
    enum UDSCommunicationControlType ctrlType; 
    enum UDSCommunicationType commType;
} UDSCommCtrlArgs_t;
```
#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request accepted  |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | The requested control type is not supported |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | The server can't enable/disable the selected communication type now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | The requested control type or communication type is erroneous |

### `UDS_EVT_WriteDataByIdent` (0x2E)

#### Arguments

```c
typedef struct {
    const uint16_t dataId;     /*! WDBI Data Identifier */
    const uint8_t *const data; /*! pointer to data */
    const uint16_t len;        /*! length of data */
} UDSWDBIArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request to write data accepted  |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | The server can't write this data now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | The requested data identifer isn't supported or the data is invalid |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | The current level of security access doesn't permit writing to the requested data identifier |
| `0x72` | `UDS_NRC_GeneralProgrammingFailure` | Memory write failed |

### `UDS_EVT_RoutineCtrl` (0x31)

#### Arguments

```c
typedef struct {
    const uint8_t ctrlType;      /*! routineControlType */
    const uint16_t id;           /*! routineIdentifier */
    const uint8_t *optionRecord; /*! optional data */
    const uint16_t len;          /*! length of optional data */
    /*! function for copying to the server send buffer. Returns `UDS_PositiveResponse` on success and `UDS_NRC_ResponseTooLong` if the length of the data to be copied exceeds that of the server send buffer */
    uint8_t (*copyStatusRecord)(UDSServer_t *srv, const void *src,
                                uint16_t len);
} UDSRoutineCtrlArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request accepted  |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | The server can't perform this operation now |
| `0x24` | `UDS_NRC_RequestSequenceError` | Stop requested but routine hasn't started. Start requested but routine has already started (optional). Results are not available becuase routine has never started. |
| `0x31` | `UDS_NRC_RequestOutOfRange` | The requested routine identifer isn't supported or the `optionRecord` is invalid |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | The current level of security access doesn't permit this operation |
| `0x72` | `UDS_NRC_GeneralProgrammingFailure` | internal memory operation failed (e.g. erasing flash) |

### `UDS_EVT_RequestDownload` (0x34)

#### Arguments

```c
typedef struct {
    const void *addr;                   /*! requested address */
    const size_t size;                  /*! requested download size */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    uint16_t maxNumberOfBlockLength; /*! response: inform client how many data bytes to send in each
                                        `TransferData` request */
} UDSRequestDownloadArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request accepted  |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | The server can't perform this operation now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | `dataFormatIdentifier` invalid, `addr` or `size` invalid |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | The current level of security access doesn't permit this operation |
| `0x34` | `UDS_NRC_AuthenticationRequired` | Client rights insufficient |
| `0x70` | `UDS_NRC_UploadDownloadNotAccepted` | download cannot be accomplished due to fault |


### `UDS_EVT_TransferData` (0x36)

#### Arguments

```c
typedef struct {
    const uint8_t *const data; /*! transfer data */
    const uint16_t len;        /*! transfer data length */
    /*! function for copying to the server send buffer. Returns `UDS_PositiveResponse` on success and `UDS_NRC_ResponseTooLong` if the length of the data to be copied exceeds that of the server send buffer */
    uint8_t (*copyResponse)(
        UDSServer_t *srv, const void *src,
        uint16_t len);
} UDSTransferDataArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request accepted  |
| `0x31` | `UDS_NRC_RequestOutOfRange` | `data` contents invalid, length incorrect |
| `0x72` | `UDS_NRC_GeneralProgrammingFailure` | Memory write failed |
| `0x92` | `UDS_NRC_VoltageTooHigh` | Can't write flash: voltage too high |
| `0x93` | `UDS_NRC_VoltageTooLow` | Can't write flash: voltage too low |

### `UDS_EVT_RequestTransferExit` (0x37)

#### Arguments

```c
typedef struct {
    const uint8_t *const data; /*! request data */
    const uint16_t len;        /*! request data length */
    /*! function for copying to the server send buffer. Returns `UDS_PositiveResponse` on success and `UDS_NRC_ResponseTooLong` if the length of the data to be copied exceeds that of the server send buffer */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len);
} UDSRequestTransferExitArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request accepted  |
| `0x31` | `UDS_NRC_RequestOutOfRange` | `data` contents invalid, length incorrect |
| `0x72` | `UDS_NRC_GeneralProgrammingFailure` | finalizing the data transfer failed |

### `UDS_SRV_EVT_RequestFileTransfer` (0x38)

#### Arguments

```c
typedef struct {
    const uint8_t modeOfOperation;      /*! requested specifier for operation mode */
    const uint16_t filePathLen;         /*! request data length */
    const uint8_t *filePath;            /*! requested file path and name */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    const size_t fileSizeUnCompressed;  /*! optional file size */
    const size_t fileSizeCompressed;    /*! optional file size */
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestFileTransferArgs_t;
```

#### Supported Responses

| Value  | enum                 | Meaning | 
| - | - | - | 
| `0x00` | `UDS_PositiveResponse` | Request accepted  |
| `0x13` | `UDS_NRC_IncorrectMessageLengthOrInvalidFormat` | Length of the message is wrong |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Downloading or uploading data is ongoing or other conditions to be able to execute this service are not met |
| `0x31` | `UDS_NRC_RequestOutOfRange` | `data` contents invalid, length incorrect |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | The server is secure |
| `0x70` | `UDS_NRC_UploadDownloadNotAccepted` | An attempt to download to a server's memory cannot be accomplished due to some fault conditions |

# Documentation: Client



## Examples

[examples/README.md](examples/README.md)

# Contributing

contributions are welcome

## Reporting Issues

When reporting issues, please state what you expected to happen.

## Running Tests

```sh
bazel test //test:all
```

See also [test_all.sh](./test_all.sh) and [test/README.md](test/README.md)

## Release

```sh
bazel build //:release
```

### Release Checklist

- [ ] version is consistent in `version.h` and `README.md`
- [ ] all tests passing

# Acknowledgements

- [`isotp-c`](https://github.com/SimonCahill/isotp-c) which this project embeds

# Changelog

## 0.8.0
- breaking API changes:
    - event enum consolidated `UDS_SRV_EVT_...` -> `UDS_EVT`
    - UDSClient refactored into event-based API
    - negative server response now raises a client error by default.
    - server NRCs prefixed with `UDS_NRC_`
    - NRCs merged into `UDS_Err` enum.
- added more examples of client usage


## 0.7.2
- runtime safety:
    1. turn off assertions by default, enable by `-DUDS_ENABLE_ASSERT`
    2. prefer `return UDS_ERR_INVALID_ARG;` over assertion in public functions
- use SimonCahill fork of isotp-c

## 0.7.1
- amalgamated sources into `iso14229.c` and `iso14229.h` to ease integration

## 0.7.0
- test refactoring. theme: test invariance across different transports and processor architectures
- breaking API changes:
    - overhauled transport layer implementation
    - simplified client and server init
    - `UDS_ARCH_` renamed to `UDS_SYS_`

## 0.6.0
- breaking API changes:
    - `UDSClientErr_t` merged into `UDSErr_t`
    - `TP_SEND_INPROGRESS` renamed to `UDS_TP_SEND_IN_PROGRESS`
    - refactored `UDSTp_t` to encourage struct inheritance
    - `UDS_TP_LINUX_SOCKET` renamed to `UDS_TP_ISOTP_SOCKET`
- added server fuzz test and qemu tests
- cleaned up example tests, added isotp-c on socketcan to examples
- added `UDS_EVT_DoScheduledReset`
- improve client error handling

## 0.5.0
- usability: refactored into a single .c/.h module
- usability: default transport layer configs are now built-in
- API cleanup: use `UDS` prefix on all exported functions
- API cleanup: use a single callback function for all server events

## 0.4.0
- refactor RDBIHandler to pass a function pointer that implements safe memmove rather than requiring the user to keep valid data around for an indefinite time or risking a buffer overflow.
- Prefer fixed-width. Avoid using `enum` types as return types and in structures.
- Transport layer is now pluggable and supports the linux kernel ISO-TP driver in addition to `isotp-c`. See [examples](./examples/README.md).

## 0.3.0
- added `iso14229ClientRunSequenceBlocking(...)`
- added server and client examples
- simplified test flow, deleted opaque macros and switch statements
- flattened client and server main structs
- simplified usage by moving isotp-c initialization parameters into server/client config structs 
- remove redundant buffers in server

## 0.2.0
- removed all instances of `__attribute__((packed))`
- refactored server download functional unit API to simplify testing
- refactored tests
    - ordered by service
    - documented macros
- removed middleware 
- simplified server routine control API
- removed redundant function `iso14229ServerEnableService`
- updated example

## 0.1.0
- Add client
- Add server SID 0x27 SecurityAccess
- API changes

## 0.0.0
- initial release
