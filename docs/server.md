# UDS Server 


## Quickstart
```c
UDSServer_t server;
UDSTp_t tp; 

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg)
{
    // your server callbacks go in here.
    return UDS_PositiveResponse;
}

int main() {
    UDSTpIsoTpSockInitServer(&tp, "vcan0", 0x7E0, 0x7E8, 0x7DF); // initialize transport for linux; see \ref examples for more platforms
    UDSServerInit(&server);
    server.tp = tp;
    server.fn = fn; 

    while (1) {
        UDSServerPoll(&server); // call UDSServerPoll at an interval of 5ms or less.
    }
}
```

The UDS server API provides functionality for implementing diagnostic services that respond to UDS client requests. The server is event-driven. Incoming client requests are processed by your service handler function (called `fn` by convention).

### Service Handler

The service handler function `server.fn` is called by `UDSServerPoll` when an event occurs. A listing of all events is available in \ref UDSEvent_t.

The handler structure has five parts:

1. Switch on the incoming \ref UDSEvent_t "event"
2. Case for a specific event
3. Cast the `arg` pointer to the type specified by \ref UDSEvent_t
4. Optionally process the arguments
5. Return a response 

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

## Server Structure

The \ref UDSServer_t structure contains:

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

## Server Events

See \ref UDSEvent_t for the mapping from event to argument type.

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

To control long-running tasks asynchronously, consider using \ref UDSSendRoutineCtrl .

## Session Management

The server tracks the current diagnostic session:

```c
if (srv->sessionType == UDS_LEV_DS_EXTDS) {
    // Extended diagnostic session is active
}
```

Sessions automatically timeout after S3 time of inactivity, returning to the default session.

Some configuration options are set at compile-time. See : \ref config.
