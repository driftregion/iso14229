# iso14229 {#mainpage}

A portable, embedabble ISO14229-1 (UDS) library for writing UDS clients and servers.

## Features

- Supports client and server implementations
- Event-driven, non-blocking API
- Pluggable transport layer (ISO-TP)
- No dynamic memory allocation
- Portable C99 code
- Comprehensive test coverage

## Quick Start

### Client Example

```c
#include "iso14229.h"

UDSClient_t client;
UDSClientInit(&client);
client.tp = /* your transport layer */;

// Send a diagnostic session control request
UDSSendDiagSessCtrl(&client, 0x01);

// Poll the client
while (client.state != UDS_CLIENT_IDLE) {
    UDSClientPoll(&client);
}
```

### Server Example

```c
#include "iso14229.h"

UDSServer_t server;
UDSServerInit(&server);
server.tp = /* your transport layer */;
server.fn = my_service_handler;

// Poll the server
while (1) {
    UDSServerPoll(&server);
}
```

## Examples

| Example | Description |
|---------|-------------|
| \ref examples/linux_server/README.md "linux_server" | Basic Linux server using socketcan ISO-TP |
| \ref examples/linux_rdbi_wdbi/README.md "linux_rdbi_wdbi" | Read/Write Data By Identifier (0x22/0x2E) |
| \ref examples/linux_server_0x27/README.md "linux_server_0x27" | Security Access (0x27) |
| \ref examples/linux_server_0x29/README.md "linux_server_0x29" | Authentication service (0x29) |
| \ref examples/arduino_server/README.md "arduino_server" | Arduino server |
| \ref examples/esp32_server/README.md "esp32_server" | ESP32 server |
| \ref examples/s32k144_server/README.md "s32k144_server" | NXP S32K144 server |

## Architecture

The library is organized into several key components:

- **Client API** (client.h) - UDS client functionality
- **Server API** (server.h) - UDS server functionality
- **Transport Layer** (tp.h) - Pluggable ISO-TP transport abstraction
- **Core UDS** (uds.h) - Common UDS definitions and utilities

## Transport Layers

The library supports multiple ISO-TP transport implementations:

- **isotp_mock** - In-memory transport for testing
- **isotp_c** - Software ISO-TP implementation using isotp-c library
- **isotp_sock** - Linux kernel ISO-TP socket
- **isotp_c_socketcan** - isotp-c over SocketCAN

## Repository

Source code: https://github.com/driftregion/iso14229

## License

MIT License - See LICENSE file for details
