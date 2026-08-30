# DoIP Client/Server Example

This example demonstrates the **DoIP (Diagnostic over IP - ISO 13400)** transport layer implementation for UDS (ISO 14229) using TCP.

## Overview

The example consists of:

- **DoIP Server** (`doip_test_server.c`) - Receives UDS diagnostic requests via DoIP and sends responses
- **DoIP Client** (`doip_test_client.c`) - Sends UDS diagnostic requests via DoIP and receives responses
- **Test Script** (`test.sh`) - Automated integration test

## Features

### Client

- Connects to DoIP server via TCP
- Performs routing activation
- Executes UDS diagnostic services (took example from `examples/linux_rdbi_wdbi`):
  - **ReadDataByIdentifier (0x22)** - Reads DID 0xF190
  - **WriteDataByIdentifier (0x2E)** - Writes incremented value to DID 0xF190
- Demonstrates async request/response handling

### Server

- Listens on TCP port 13400 (standard DoIP port)
- UDP discovery is not supported (and not needed)
- Handles routing activation requests
- Processes UDS diagnostic messages:
  - ReadDataByIdentifier (0x22)
  - WriteDataByIdentifier (0x2E)
  - DiagnosticSessionControl (0x10)
- Sends DoIP diagnostic message ACK/NACK
- Responds to alive check requests

## Building

```bash
make
```

This builds two executables:

- `doip_server` - DoIP server
- `doip_client` - DoIP client

## Running

### Manual Test

Start the server in one terminal:

```bash
./doip_server
```

Run the client in another terminal:

```bash
./doip_client
```

The client will:

1. Connect to server at `127.0.0.1:13400`
2. Activate routing
3. Read DID 0xF190
4. Write DID 0xF190 with incremented value
5. Exit

### Automated Test

```bash
./test.sh
```

The test script:

1. Builds both client and server
2. Starts server in background
3. Runs client and waits for completion
4. Verifies client exited successfully (exit code 0)
5. Verifies server is still running
6. Cleans up

## Configuration

Edit `Makefile` to adjust log level:

```makefile
# Debug build with verbose logging
CFLAGS = -DUDS_TP_DOIP=1 -DUDS_LOG_LEVEL=UDS_LOG_VERBOSE

# Release build with info logging
CFLAGS = -DUDS_TP_DOIP=1 -DUDS_LOG_LEVEL=UDS_LOG_INFO
```

## Network Details

- **Protocol**: DoIP over TCP (ISO 13400)
- **Port**: 13400
- **Server IP**: 127.0.0.1 (localhost)
- **Source Address**: 0x0E00 (client logical address)
- **Target Address**: 0x4001 (server logical address)

## DoIP Message Flow

```text
Client                          Server
  |                               |
  |------- TCP Connect ---------->|
  |                               |
  |--- Routing Activation Req --->|
  |<-- Routing Activation Res ----|
  |                               |
  |--- Diagnostic Message ------->|
  |<-- Diagnostic Message ACK ----|
  |<-- Diagnostic Response -------|
  |                               |
  |--- Diagnostic Message ------->|
  |<-- Diagnostic Message ACK ----|
  |<-- Diagnostic Response -------|
  |                               |
  |------- TCP Close ------------>|
```

## Troubleshooting

**Server won't start:**

- Check if port 13400 is already in use: `netstat -tuln | grep 13400`
- Kill any existing server: `pkill doip_server`

**Client can't connect:**

- Verify server is running: `ps aux | grep doip_server`
- Check firewall settings
- Use `tcpdump` to inspect traffic: `sudo tcpdump -i lo port 13400 -X`

**Test script fails:**

- Check exit codes in test output
- Run with verbose logging (edit Makefile)
- Run server and client manually to isolate issues

## References

- ISO 13400: Road vehicles - Diagnostic communication over Internet Protocol (DoIP)
- ISO 14229: Unified Diagnostic Services (UDS)

---

## New Modules and Functions (DoIP Transport & Discovery)

This codebase now includes a small DoIP transport abstraction, a split TCP/UDP implementation, and basic UDP discovery with a selection callback.

- Transport Abstraction
  - File: [src/tp/doip/doip_transport.h](../../src/tp/doip/doip_transport.h)
    - `DoIPTransport`: minimal socket wrapper used by the client (fields: `fd`, `port`, `ip`, `is_udp`, `loopback`).
  - TCP Transport: [src/tp/doip/doip_tp_tcp.c](../../src/tp/doip/doip_tp_tcp.c)
    - `doip_tp_tcp_init(t, ip, port)`: initialize transport with remote IP/port.
    - `doip_tp_tcp_connect(t)`: connect to DoIP TCP server.
    - `doip_tp_tcp_send(t, buf, len)`: send bytes.
    - `doip_tp_tcp_recv(t, buf, len, timeout_ms)`: receive with timeout.
    - `doip_tp_tcp_close(t)`: close socket.
  - UDP Transport: [src/tp/doip/doip_tp_udp.c](../../src/tp/doip/doip_tp_udp.c)
    - `doip_tp_udp_init(t, port, loopback)`: bind for discovery (loopback binds 127.0.0.1).
    - `doip_tp_udp_join_default_multicast(t)`: join 224.224.224.224:13400.
    - `doip_tp_udp_recv(t, buf, len, timeout_ms)`: receive datagram.
    - `doip_tp_udp_recvfrom(t, buf, len, timeout_ms, src_ip, src_ip_sz, src_port)`: receive + source address.
    - `doip_tp_udp_close(t)`: close socket.

- DoIP Client Refactor
  - Files: [src/tp/doip/doip_client.h](../../src/tp/doip/doip_client.h), [src/tp/doip/doip_client.c](../../src/tp/doip/doip_client.c)
  - The client now embeds `DoIPTransport` for TCP (diagnostics) and UDP (discovery) and uses these for connect/send/recv/close.
  - Field `udp_loopback` controls whether discovery uses loopback instead of multicast.

- Discovery & Server Selection
  - Types (in client header):
    - `DoIPDiscoveryInfo`: `{ ip, remote_port, logical_address, vin[18], eid[13], gid[13] }`.
    - `DoIPSelectServerFn`: `bool (*)(const DoIPDiscoveryInfo*, void*)` callback.
  - APIs:
    - `UDSDoIPSetSelectionCallback(tp, fn, user)`: register callback to choose a server from responders.
    - `UDSDoIPDiscoverVehicles(tp, timeout_ms, loopback)`: listens for discovery frames (multicast or loopback), parses VIN/EID/GID when present, invokes selection callback (or defaults to first responder), and updates `tp->server_ip`/`tp->server_port` on selection.
  - Notes:
    - Default DoIP multicast group/port: 224.224.224.224:13400.
    - Default selection without a callback picks the first responder; prefer a callback to filter by VIN/logical address/EID/GID.

- Example
  - File: [examples/doip_discovery_example/main.c](../doip_discovery_example/main.c)
    - Demonstrates discovery and selection via VIN prefix. Prints the selected server IP.

- Build Integration
  - CMake: when `BUILD_UDS_TP_DOIP=ON`, the DoIP target includes the new TCP/UDP transport sources.
  - Bazel: `src/BUILD` updated to include the new transport sources and headers.

  ### Quick Start: Discovery + Selection

  Minimal example of discovering DoIP responders and selecting one by VIN prefix:

  ```c
  #include <stdio.h>
  #include <string.h>
  #include <stdbool.h>
  #include <stdint.h>
  #include "tp/doip/doip_client.h"

  static bool select_by_vin(const DoIPDiscoveryInfo *info, void *user) {
      const char *needle = (const char *)user; // VIN prefix or full VIN
      if (!needle || !*needle) return false;
      if (info->vin[0] == '\0') return false;
      return strncmp(info->vin, needle, strlen(needle)) == 0;
  }

  int main(int argc, char **argv) {
      const char *vin_prefix = argc > 1 ? argv[1] : NULL;
      bool loopback = argc > 2 ? (strcmp(argv[2], "loopback") == 0) : false;

      DoIPClient_t tp;
      memset(&tp, 0, sizeof(tp));

      UDSDoIPSetSelectionCallback(&tp, select_by_vin, (void*)vin_prefix);

      int count = UDSDoIPDiscoverVehicles(&tp, 2000, loopback);
      printf("Discovered %d responders\n", count);
      printf("Selected server: %s\n", tp.server_ip);
      return 0;
  }
  ```

  Build and run (ad hoc):

  ```bash
  gcc -DUDS_TP_DOIP -Isrc -I. -o build/doip_discovery_example \
    src/tp/doip/doip_client.c src/tp/doip/doip_tp_udp.c src/tp/doip/doip_tp_tcp.c \
    examples/doip_discovery_example/main.c

  # Default multicast discovery (~2s), chooses first responder
  ./build/doip_discovery_example

  # Filter by VIN prefix
  ./build/doip_discovery_example WBA

  # Loopback discovery for local testing
  ./build/doip_discovery_example "" loopback
  ```
