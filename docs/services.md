# UDS Services {#services}

This page documents the services supported by iso14229.

## Supported Services {#supported-services}

| Service ID | Service Name | Server | Client | Standard Responses* |
|------------|--------------|--------|--------|----------------------|
| 0x10 | \ref service_0x10 "Diagnostic Session Control" | ✅ | ✅ | \ref service_0x10_supported_responses "NRCs" |
| 0x11 | \ref service_0x11 "ECU Reset" | ✅ | ✅ | \ref service_0x11_supported_responses "NRCs" |
| 0x14 | \ref service_0x14 "Clear Diagnostic Information" | ✅ | ❌ | \ref service_0x14_supported_responses "NRCs" |
| 0x19 | Read DTC Information | ❌ | ❌ | |
| 0x22 | \ref service_0x22 "Read Data By Identifier" | ✅ | ✅ | \ref service_0x22_supported_responses "NRCs" |
| 0x23 | Read Memory By Address | ❌ | ❌ | |
| 0x24 | Read Scaling Data By Identifier | ❌ | ❌ | |
| 0x27 | \ref service_0x27 "Security Access" | ✅ | ✅ | \ref service_0x27_supported_responses "NRCs" |
| 0x28 | \ref service_0x28 "Communication Control" | ✅ | ✅ | \ref service_0x28_supported_responses "NRCs" |
| 0x2A | Read Periodic Data By Identifier | ❌ | ❌ | |
| 0x2C | Dynamically Define Data Identifier | ❌ | ❌ | |
| 0x2E | \ref service_0x2e "Write Data By Identifier" | ✅ | ✅ | \ref service_0x2e_supported_responses "NRCs" |
| 0x2F | Input/Output Control By Identifier | ✅ | ❌ | |
| 0x31 | \ref service_0x31 "Routine Control" | ✅ | ✅ | \ref service_0x31_supported_responses "NRCs" |
| 0x34 | \ref service_0x34 "Request Download" | ✅ | ✅ | \ref service_0x34_supported_responses "NRCs" |
| 0x35 | Request Upload | ✅ | ✅ | |
| 0x36 | \ref service_0x36 "Transfer Data" | ✅ | ✅ | \ref service_0x36_supported_responses "NRCs" |
| 0x37 | \ref service_0x37 "Request Transfer Exit" | ✅ | ✅ | \ref service_0x37_supported_responses "NRCs" |
| 0x38 | Request File Transfer | ✅ | ✅ | |
| 0x3D | Write Memory By Address | ✅ | ❌ | |
| 0x3E | Tester Present | ✅ | ✅ | |
| 0x83 | Access Timing Parameter | ❌ | ❌ | |
| 0x84 | Secured Data Transmission | ❌ | ❌ | |
| 0x85 | Control DTC Setting | ✅ | ✅ | |
| 0x86 | Response On Event | ❌ | ❌ | |
| 0x87 | Link Control | ✅ | ❌ | |

### Standard Responses* {#standard_responses}

The standard lists a set of supported NRCs for each service. In your server implementation, it is recommended to use to these whenever possible.

---

## 0x10 Diagnostic Session Control {#service_0x10}

Change the diagnostic session type.

### Server Event

`UDS_EVT_DiagSessCtrl`

### Arguments

```c
typedef struct {
    const uint8_t type;  /*! requested session type */
    uint16_t p2_ms;      /*! optional return value: p2 timing override */
    uint32_t p2_star_ms; /*! optional return value: p2* timing override */
} UDSDiagSessCtrlArgs_t;
```

### Session Types

| Value | Define | Description |
|-------|--------|-------------|
| 0x01 | `UDS_LEV_DS_DS` | Default Session |
| 0x02 | `UDS_LEV_DS_PRGS` | Programming Session |
| 0x03 | `UDS_LEV_DS_EXTDS` | Extended Diagnostic Session |
| 0x04 | `UDS_LEV_DS_SSDS` | Safety System Diagnostic Session |

### Supported Responses {#service_0x10_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Request accepted |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | Requested session type not supported |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Conditions not correct |

### Client API

\ref UDSSendDiagSessCtrl

---

## 0x11 ECU Reset {#service_0x11}

Request ECU reset.

### Server Event

`UDS_EVT_EcuReset`

### Arguments

```c
typedef struct {
    const uint8_t type;           /*! reset type */
    uint32_t powerDownTimeMillis; /*! delay before reset event */
} UDSECUResetArgs_t;
```

### Reset Types

| Value | Define | Description |
|-------|--------|-------------|
| 0x01 | `UDS_LEV_RT_HR` | Hard Reset |
| 0x02 | `UDS_LEV_RT_KOFFONR` | Key Off On Reset |
| 0x03 | `UDS_LEV_RT_SR` | Soft Reset |
| 0x04 | `UDS_LEV_RT_ERPSD` | Enable Rapid Power Shutdown |
| 0x05 | `UDS_LEV_RT_DRPSD` | Disable Rapid Power Shutdown |

### Supported Responses {#service_0x11_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Reset will occur |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | Reset type not supported |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Conditions not correct for reset |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | Security access required |

### Client API

\ref UDSSendECUReset

---

## 0x14 Clear Diagnostic Information {#service_0x14}

Clear diagnostic trouble codes.

### Server Event

`UDS_EVT_ClearDiagnosticInfo`

### Arguments

```c
typedef struct {
    const uint32_t groupOfDTC;     /*! DTC group to clear */
    const bool hasMemorySelection; /*! memory selection present */
    const uint8_t memorySelection; /*! memory selection (optional) */
} UDSCDIArgs_t;
```

### Supported Responses {#service_0x14_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | DTCs cleared |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Cannot clear DTCs now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | Invalid DTC group |

---

## 0x22 Read Data By Identifier {#service_0x22}

Read data identified by a 16-bit identifier.

### Server Event

`UDS_EVT_ReadDataByIdent`

### Arguments

```c
typedef struct {
    const uint16_t dataId; /*! data identifier */
    uint8_t (*copy)(UDSServer_t *srv, const void *src, uint16_t count);
} UDSRDBIArgs_t;
```

### Supported Responses {#service_0x22_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Data returned successfully |
| `0x14` | `UDS_NRC_ResponseTooLong` | Response exceeds buffer |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Cannot read data now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | Data identifier not supported |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | Security access required |

### Client API

\ref UDSSendRDBI, \ref UDSUnpackRDBIResponse

### Example

See \ref examples/linux_rdbi_wdbi/README.md

---

## 0x27 Security Access {#service_0x27}

Unlock security-protected diagnostic services.

### Server Events

- `UDS_EVT_SecAccessRequestSeed` - Client requests seed
- `UDS_EVT_SecAccessValidateKey` - Client sends key for validation

### Arguments

Request Seed:
```c
typedef struct {
    const uint8_t level;             /*! security level */
    const uint8_t *const dataRecord; /*! request data */
    const uint16_t len;              /*! data length */
    uint8_t (*copySeed)(UDSServer_t *srv, const void *src, uint16_t len);
} UDSSecAccessRequestSeedArgs_t;
```

Validate Key:
```c
typedef struct {
    const uint8_t level;      /*! security level */
    const uint8_t *const key; /*! key to validate */
    const uint16_t len;       /*! key length */
} UDSSecAccessValidateKeyArgs_t;
```

### Security Levels

Odd levels (0x01, 0x03, ...) request seed, even levels (0x02, 0x04, ...) send key.

### Supported Responses {#service_0x27_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Seed provided or key accepted |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | Security level not supported |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Wrong sequence |
| `0x24` | `UDS_NRC_RequestSequenceError` | Request out of sequence |
| `0x35` | `UDS_NRC_InvalidKey` | Key verification failed |
| `0x36` | `UDS_NRC_ExceedNumberOfAttempts` | Too many failed attempts |
| `0x37` | `UDS_NRC_RequiredTimeDelayNotExpired` | Must wait before retry |

### Client API

\ref UDSSendSecurityAccess, \ref UDSUnpackSecurityAccessResponse

### Example

See \ref examples/linux_server_0x27/README.md

---

## 0x28 Communication Control {#service_0x28}

Control communication messages.

### Server Event

`UDS_EVT_CommCtrl`

### Arguments

```c
typedef struct {
    uint8_t ctrlType; /*! control type */
    uint8_t commType; /*! communication type */
    uint16_t nodeId;  /*! node ID (optional) */
} UDSCommCtrlArgs_t;
```

### Control Types

| Value | Define | Description |
|-------|--------|-------------|
| 0x00 | `UDS_LEV_CTRLTP_ERXTX` | Enable RX and TX |
| 0x01 | `UDS_LEV_CTRLTP_ERXDTX` | Enable RX, Disable TX |
| 0x02 | `UDS_LEV_CTRLTP_DRXETX` | Disable RX, Enable TX |
| 0x03 | `UDS_LEV_CTRLTP_DRXTX` | Disable RX and TX |

### Supported Responses {#service_0x28_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Control accepted |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | Control type not supported |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Cannot apply control |
| `0x31` | `UDS_NRC_RequestOutOfRange` | Invalid parameters |

### Client API

\ref UDSSendCommCtrl

---

## 0x2E Write Data By Identifier {#service_0x2e}

Write data identified by a 16-bit identifier.

### Server Event

`UDS_EVT_WriteDataByIdent`

### Arguments

```c
typedef struct {
    const uint16_t dataId;     /*! data identifier */
    const uint8_t *const data; /*! data to write */
    const uint16_t len;        /*! data length */
} UDSWDBIArgs_t;
```

### Supported Responses {#service_0x2e_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Data written successfully |
| `0x13` | `UDS_NRC_IncorrectMessageLengthOrInvalidFormat` | Invalid data length |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Cannot write now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | Data identifier not supported |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | Security access required |

### Client API

\ref UDSSendWDBI

### Example

See \ref examples/linux_rdbi_wdbi/README.md

---

## 0x31 Routine Control {#service_0x31}

Start, stop, or request results from server-side routines.

### Server Event

`UDS_EVT_RoutineCtrl`

### Arguments

```c
typedef struct {
    const uint8_t ctrlType;      /*! routine control type */
    const uint16_t id;           /*! routine identifier */
    const uint8_t *optionRecord; /*! optional data */
    const uint16_t len;          /*! option data length */
    uint8_t (*copyStatusRecord)(UDSServer_t *srv, const void *src, uint16_t len);
} UDSRoutineCtrlArgs_t;
```

### Control Types

| Value | Define | Description |
|-------|--------|-------------|
| 0x01 | `UDS_LEV_RCTP_STR` | Start Routine |
| 0x02 | `UDS_LEV_RCTP_STPR` | Stop Routine |
| 0x03 | `UDS_LEV_RCTP_RRR` | Request Routine Results |

### Supported Responses {#service_0x31_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Routine control accepted |
| `0x12` | `UDS_NRC_SubFunctionNotSupported` | Control type not supported |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Cannot execute routine |
| `0x24` | `UDS_NRC_RequestSequenceError` | Wrong sequence |
| `0x31` | `UDS_NRC_RequestOutOfRange` | Routine ID not supported |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | Security access required |

### Client API

\ref UDSSendRoutineCtrl, \ref UDSUnpackRoutineControlResponse

---

## 0x34 Request Download {#service_0x34}

Initiate data download to server (flash programming, calibration update).

### Server Event

`UDS_EVT_RequestDownload`

### Arguments

```c
typedef struct {
    const void *addr;                   /*! memory address */
    const size_t size;                  /*! download size */
    const uint8_t dataFormatIdentifier; /*! data format */
    uint16_t maxNumberOfBlockLength;    /*! max block size response */
} UDSRequestDownloadArgs_t;
```

### Supported Responses {#service_0x34_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Download accepted |
| `0x22` | `UDS_NRC_ConditionsNotCorrect` | Cannot download now |
| `0x31` | `UDS_NRC_RequestOutOfRange` | Invalid address/size |
| `0x33` | `UDS_NRC_SecurityAccessDenied` | Security access required |
| `0x70` | `UDS_NRC_UploadDownloadNotAccepted` | Download rejected |

### Client API

\ref UDSSendRequestDownload, \ref UDSUnpackRequestDownloadResponse

---

## 0x36 Transfer Data {#service_0x36}

Transfer data blocks after Request Download/Upload.

### Server Event

`UDS_EVT_TransferData`

### Arguments

```c
typedef struct {
    const uint8_t *const data; /*! transfer data */
    const uint16_t len;        /*! data length */
    const uint16_t maxRespLen; /*! max response length */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src, uint16_t len);
} UDSTransferDataArgs_t;
```

### Supported Responses {#service_0x36_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Data transferred |
| `0x24` | `UDS_NRC_RequestSequenceError` | No active transfer |
| `0x71` | `UDS_NRC_TransferDataSuspended` | Transfer suspended |
| `0x72` | `UDS_NRC_GeneralProgrammingFailure` | Programming failed |
| `0x73` | `UDS_NRC_WrongBlockSequenceCounter` | Block sequence error |

### Client API

\ref UDSSendTransferData

---

## 0x37 Request Transfer Exit {#service_0x37}

Terminate data transfer.

### Server Event

`UDS_EVT_RequestTransferExit`

### Arguments

```c
typedef struct {
    const uint8_t *const data; /*! request data */
    const uint16_t len;        /*! data length */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src, uint16_t len);
} UDSRequestTransferExitArgs_t;
```

### Supported Responses {#service_0x37_supported_responses}

| Value | Enum | Meaning |
|-------|------|---------|
| `0x00` | `UDS_PositiveResponse` | Transfer exited successfully |
| `0x24` | `UDS_NRC_RequestSequenceError` | No active transfer |
| `0x72` | `UDS_NRC_GeneralProgrammingFailure` | Exit processing failed |

### Client API

\ref UDSSendRequestTransferExit

---

## See Also

- \ref client "Client API"
- \ref server "Server API"
- ISO 14229-1:2020 - Road vehicles - Unified diagnostic services (UDS)
