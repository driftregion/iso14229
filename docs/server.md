# UDS Server {#server}

The UDS server API provides functionality for implementing diagnostic services that respond to UDS client requests. The server is event-driven. Incoming client requests are processed by your service handler function (called `fn` by convention).

## Basic Usage

### Initialization

```c
UDSServer_t server;
UDSTp_t *transport = /* initialize your transport */;

UDSServerInit(&server);
server.tp = transport;
server.fn = fn; /* your service handler function */
```

For transport initialization, see \ref transport_layers.

### Service Handler

The service handler function `server.fn` is called by `UDSServerPoll` when an event occurs. A listing of all events is available in \ref UDSEvent_t.

The handler structure has five parts:

1. Switch on the incoming \ref UDSEvent_t "event"
2. Case for a specific event
3. Cast the `arg` pointer to the type specified by \ref UDSEvent_t
4. Optionally process the arguments
5. Return a response (see \ref standard_responses)

Example Handler:

```c
UDSErr_t fn(UDSServer_t *srv, UDSEvent_t event, void *arg) {

    // 1: Switch on the incoming event
    switch (event) {

        // 2: Case for a specific event: The client has called 0x10 DiagnosticSessionControl
        case UDS_EVT_DiagSessCtrl: {

            // 3: Cast the arg pointer to the type specified by UDSEvent_t
            UDSDiagSessCtrlArgs_t *r= (UDSDiagSessCtrlArgs_t *)arg;

            // 4: Optionally process the arguments

            // 5: Return a response
            return UDS_OK;
        }

        // 2: Case for a specific event: The client has called 0x22 ReadDataByIdentifier
        case UDS_EVT_ReadDataByIdent: {

            // 3: Cast the arg pointer to the type specified by UDSEvent_t
            UDSWDBIArgs_t *r = (UDSWDBIArgs_t *)arg;

            // 4: Check the requested data ID 
            switch (r->dataId) {
                case 0x1234: {
                    uint8_t data[] = {0x01, 0x02, 0x03};
                    // 5: Return a response
                    return r->copy(srv, data, sizeof(data));
                    break;
                }
                default:
                    // 5: Return a response
                    return UDS_NRC_RequestOutOfRange;
            }
        }

        // ... handle other services
        default:
            return UDS_NRC_ServiceNotSupported;
    }
}
```

### Main Loop

It is recommended to call `UDSServerPoll` at an interval of 5ms or less. 

```c
while (1) {
    UDSServerPoll(&server);
}
```

## Server Structure

The \ref UDSServer structure contains:

| Identifier | Description | How to Use |
|------------|-------------|------------|
| `tp` | Pointer to ISO-TP transport layer | Set during initialization: `server.tp = transport;` |
| `fn` | Event handler callback function | Set during initialization: `server.fn = fn;` |
| `fn_data` | User data bound to server, accessible in `fn`| Optional: `server.fn_data = &my_data;` |
| `p2_ms` | P2 timeout in milliseconds | Internal use only; Set default with `UDS_SERVER_DEFAULT_P2_MS` |
| `p2_star_ms` | P2* timeout in milliseconds | Internal use only; Set default with `UDS_SERVER_DEFAULT_P2_STAR_MS` |
| `s3_ms` | S3 session timeout in milliseconds | Internal use only; Set default with `UDS_SERVER_DEFAULT_S3_MS` |
| `sessionType` | Current diagnostic session | Read: `if (server.sessionType == UDS_LEV_DS_EXTDS)` |
| `securityLevel` | Current security level | Read/write: `server.securityLevel = args->level;` |
| `xferIsActive` | Transfer operation active flag | Read: `if (server.xferIsActive)` |
| `xferBlockSequenceCounter` | Transfer block sequence counter | Read only |
| `r` | Current request/response buffers | Internal use only |

## Service Events

The server emits events for each UDS service. See \ref services "UDS Services" for detailed documentation of each service.

| Event | Service | Argument Type |
|-------|---------|---------------|
| `UDS_EVT_DiagSessCtrl` | \ref service_0x10 "0x10 Diagnostic Session Control" | \ref UDSDiagSessCtrlArgs_t |
| `UDS_EVT_EcuReset` | \ref service_0x11 "0x11 ECU Reset" | \ref UDSECUResetArgs_t |
| `UDS_EVT_ClearDiagnosticInfo` | \ref service_0x14 "0x14 Clear DTC Information" | \ref UDSCDIArgs_t |
| `UDS_EVT_ReadDTCInformation` | 0x19 Read DTC Information | \ref UDSRDTCIArgs_t |
| `UDS_EVT_ReadDataByIdent` | \ref service_0x22 "0x22 Read Data By Identifier" | \ref UDSRDBIArgs_t |
| `UDS_EVT_ReadMemByAddr` | 0x23 Read Memory By Address | \ref UDSReadMemByAddrArgs_t |
| `UDS_EVT_SecAccessRequestSeed` | \ref service_0x27 "0x27 Security Access (Request Seed)" | \ref UDSSecAccessRequestSeedArgs_t |
| `UDS_EVT_SecAccessValidateKey` | \ref service_0x27 "0x27 Security Access (Send Key)" | \ref UDSSecAccessValidateKeyArgs_t |
| `UDS_EVT_CommCtrl` | \ref service_0x28 "0x28 Communication Control" | \ref UDSCommCtrlArgs_t |
| `UDS_EVT_WriteDataByIdent` | \ref service_0x2e "0x2E Write Data By Identifier" | \ref UDSWDBIArgs_t |
| `UDS_EVT_IOControl` | 0x2F Input/Output Control | \ref UDSIOCtrlArgs_t |
| `UDS_EVT_RoutineCtrl` | \ref service_0x31 "0x31 Routine Control" | \ref UDSRoutineCtrlArgs_t |
| `UDS_EVT_RequestDownload` | \ref service_0x34 "0x34 Request Download" | \ref UDSRequestDownloadArgs_t |
| `UDS_EVT_RequestUpload` | 0x35 Request Upload | \ref UDSRequestUploadArgs_t |
| `UDS_EVT_TransferData` | \ref service_0x36 "0x36 Transfer Data" | \ref UDSTransferDataArgs_t |
| `UDS_EVT_RequestTransferExit` | \ref service_0x37 "0x37 Request Transfer Exit" | \ref UDSRequestTransferExitArgs_t |
| `UDS_EVT_RequestFileTransfer` | 0x38 Request File Transfer | \ref UDSRequestFileTransferArgs_t |
| `UDS_EVT_WriteMemByAddr` | 0x3D Write Memory By Address | \ref UDSWriteMemByAddrArgs_t |
| `UDS_EVT_ControlDTCSetting` | 0x85 Control DTC Setting | \ref UDSControlDTCSettingArgs_t |
| `UDS_EVT_LinkControl` | 0x87 Link Control | \ref UDSLinkCtrlArgs_t |

## Responding to Requests

### Positive Response

Return `UDS_OK` or use the `copy` function to send data:

```c
case UDS_EVT_ReadDataByIdent: {
    UDSRDBIArgs_t *args = (UDSRDBIArgs_t *)arg;
    uint8_t vin[] = "WBADT43452G123456";
    return args->copy(srv, vin, sizeof(vin) - 1);
}
```

### Negative Response

Return a Negative Response Code (NRC):

```c
case UDS_EVT_RoutineCtrl: {
    UDSRoutineCtrlArgs_t *args = (UDSRoutineCtrlArgs_t *)arg;
    if (args->id != 0x1234) {
        return UDS_NRC_RequestOutOfRange;
    }
    return UDS_OK;
}
```

### Response Pending

Return `UDS_NRC_RequestCorrectlyReceived_ResponsePending` (0x78) to indicate that processing is taking longer than P2:

```c
case UDS_EVT_RoutineCtrl: {
    if (routine_still_running) {
        return UDS_NRC_RequestCorrectlyReceived_ResponsePending;
    }
    return UDS_OK;
}
```

This is used to prevent the client from timing out during long-running server actions such as writing to flash memory.

To control long-running tasks asynchronously, consider using \ref service_0x31.

## Session Management

The server tracks the current diagnostic session:

```c
if (srv->sessionType == UDS_LEV_DS_EXTDS) {
    // Extended diagnostic session is active
}
```

Sessions automatically timeout after S3 time of inactivity, returning to the default session.

## Security Access

See \ref examples/linux_server_0x27/server.c "Security Access Server Example"

## Configuration {#server_configuration}

Server behavior can be configured at compile-time:

| Define | Default | Description |
|--------|---------|-------------|
| `UDS_SERVER_DEFAULT_P2_MS` | 50 | Default P2 timeout (ms) |
| `UDS_SERVER_DEFAULT_P2_STAR_MS` | 5000 | Default P2* timeout (ms) |
| `UDS_SERVER_DEFAULT_S3_MS` | 5100 | Session timeout (ms) |
| `UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS` | 60 | Delay before ECU reset (ms) |
| `UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS` | 1000 | Boot delay for security access (ms) |
| `UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS` | 1000 | Delay after auth failure (ms) |
| `UDS_SERVER_SEND_BUF_SIZE` | 4095 | Send buffer size |
| `UDS_SERVER_RECV_BUF_SIZE` | 4095 | Receive buffer size |

## See Also

- \ref client.h "Client API"
- \ref tp.h "Transport Layer API"
- \ref examples/linux_server/README.md "Basic Server Example"
- \ref examples/linux_rdbi_wdbi/server.c "RDBI/WDBI Server Example"
- \ref examples/linux_server_0x27/server.c "Security Access Server Example"
