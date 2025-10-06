# UDS Client {#client}

The UDS client API provides functionality for sending diagnostic requests to UDS servers.

## Basic Usage

### Initialization

```c
UDSClient_t client;
UDSTp_t *transport = /* initialize your transport */;

UDSClientInit(&client);
client.tp = transport;
```

For transport initialization, see \ref transport_layers.

### Sending Requests

```c
// Send Diagnostic Session Control
UDSSendDiagSessCtrl(&client, UDS_LEV_DS_EXTDS);

// Send Read Data By Identifier
uint16_t dids[] = {0xF190};
UDSSendRDBI(&client, dids, 1);

// Send ECU Reset
UDSSendECUReset(&client, UDS_LEV_RT_HR);
```

### Processing Responses

```c
while (client.state != UDS_CLIENT_IDLE) {
    UDSClientPoll(&client);
    // Handle events in callback
}
```

## Client Structure

The \ref UDSClient structure contains:

- **Timeouts**: `p2_ms`, `p2_star_ms` - Server response timing parameters
- **Transport**: `tp` - Pointer to ISO-TP transport layer
- **State**: `state` - Current client state
- **Options**: `options`, `defaultOptions` - Request behavior flags
- **Callback**: `fn`, `fn_data` - Event handler and user data
- **Buffers**: `recv_buf`, `send_buf` - Internal message buffers

## Request Options

Combine these flags when sending requests:

| Flag | Description |
|------|-------------|
| `UDS_SUPPRESS_POS_RESP` | Suppress positive response (0x80 bit) |
| `UDS_FUNCTIONAL` | Send as functional request (broadcast) |
| `UDS_IGNORE_SRV_TIMINGS` | Ignore the server-provided P2/P2* values returned by a successful call to DiagnosticSessionControl |

Example:
```c
client.options = UDS_SUPPRESS_POS_RESP | UDS_FUNCTIONAL;
UDSSendTesterPresent(&client);
// client.options is cleared automatically after each request
```

## Event-Driven API

The client uses callbacks to notify the application of events:

```c
int fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    switch (evt) {
        case UDS_EVT_SendComplete:
            // Request sent successfully
            break;
        case UDS_EVT_ResponseReceived:
            // Response received
            break;
        case UDS_EVT_Err:
            // Error occurred
            UDSErr_t *err = (UDSErr_t *)ev_data;
            printf("Error: %s\n", UDSErrToStr(*err));
            break;
    }
    return 0;
}

client.fn = client_callback;
```

## Supported Services

See \ref services "UDS Services".

## Unpacking Responses

Helper functions are provided to parse complex responses:

```c
// Security Access response
struct SecurityAccessResponse resp;
UDSUnpackSecurityAccessResponse(&client, &resp);

// Request Download response
struct RequestDownloadResponse dl_resp;
UDSUnpackRequestDownloadResponse(&client, &dl_resp);

// Routine Control response
struct RoutineControlResponse rc_resp;
UDSUnpackRoutineControlResponse(&client, &rc_resp);

// Read Data By Identifier response
UDSRDBIVar_t vars[] = {
    {.did = 0xF190, .data = buffer, .len = sizeof(buffer)}
};
UDSUnpackRDBIResponse(&client, vars, 1);
```

## Configuration {#client_configuration}

Client behavior can be configured at compile-time:

| Define | Default | Description |
|--------|---------|-------------|
| `UDS_CLIENT_DEFAULT_P2_MS` | 150 | Default P2 timeout (ms) |
| `UDS_CLIENT_DEFAULT_P2_STAR_MS` | 1500 | Default P2* timeout (ms) |
| `UDS_CLIENT_SEND_BUF_SIZE` | 4095 | Send buffer size |
| `UDS_CLIENT_RECV_BUF_SIZE` | 4095 | Receive buffer size |

## See Also

- \ref server.h "Server API"
- \ref tp.h "Transport Layer API"
- \ref examples/linux_rdbi_wdbi/client.c "RDBI/WDBI Client Example"
- \ref examples/linux_server_0x27/client.c "Security Access Client Example"
