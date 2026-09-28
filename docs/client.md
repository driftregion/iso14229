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

## Client Configuration 

Some client behavior is configurable at runtime.
After initialization, the library never modifies the values of these flags.

| Option | Description | Valid Range | Default Value |
|-|-|-|
| `cfg_suppress_pos_resp`   | When sending requests, ask that the server not send positive responses (0x80 bit) | 0-1 | 0 | 
| `cfg_send_functional`     | Send requests as functional (broadcast) | 0-1 | 0 | 
| `cfg_ignore_srv_sess_timing` | Ignore the server-provided P2/P2* values returned by a successful call to DiagnosticSessionControl | 0-1 | 0 | 
| `cfg_data_format_identifier` | See Upload/Download functional unit | 0-255 | 0 | 
| `cfg_file_size_parameter_length` | See Upload/Download functional unit | 0-255 | 4 | 

Example:
```c
client.suppress_pos_resp = 1;
UDSSendTesterPresent(&client); // sends 
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

Some configuration options are set at compile-time. See : \ref config.