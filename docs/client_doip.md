# DoIP Client {#client_doip}

This document describes the DoIP (ISO 13400) client and transport usage in this codebase, focusing on TCP diagnostics, UDP vehicle discovery, and the DoIP transport layer.

## Basic Usage

### Discovery and Selection

Use UDP discovery to locate vehicles and select one based on VIN or first responder.

```c
#include "tp/doip/doip_client.h"

DoIPClient_t tp;
UDSDoIPSetDiscoveryOptions(/*request_only=*/true, /*dump_raw=*/false);
UDSDoIPSetSelectionCallback(&tp, /*optional*/ NULL, NULL);

// Loopback discovery (binds 13401, sends VI request to 13400)
int found = UDSDoIPDiscoverVehicles(&tp, /*timeout_ms=*/2000, /*loopback=*/true);
if (found <= 0 || tp.server_ip[0] == '\0') {
    // handle no selection
}

// Selected server is available in tp.server_ip and tp.server_port
```

To filter by VIN prefix:

```c
static bool select_by_vin(const DoIPDiscoveryInfo *info, void *user) {
    const char *prefix = (const char*)user;
    if (!prefix || !*prefix || info->vin[0] == '\0') return false;
    return strncmp(info->vin, prefix, strlen(prefix)) == 0;
}

UDSDoIPSetSelectionCallback(&tp, select_by_vin, (void*)"WVWZZZ");
int found = UDSDoIPDiscoverVehicles(&tp, 2000, true);
```

### Initialization and Routing Activation

After discovery, initialize the TCP connection and activate routing:

```c
// Provide source/target logical addresses
uint16_t SA = 0x0E00; // example tester address
uint16_t TA = 0x0E80; // example server address

UDSErr_t rc = UDSDoIPInitClient(&tp, tp.server_ip, tp.server_port, SA, TA);
if (rc != UDS_OK) {
    // handle error
}

// If needed later:
rc = UDSDoIPActivateRouting(&tp);
```

### Sending and Receiving Diagnostic Messages

Once routing is active, the higher-level UDS client can use the DoIP transport (via `UDSTp_t` inside `DoIPClient_t`) to send UDS requests and receive responses. See the generic client flow in [docs/client.md](client.md) and the DoIP transport integration in [src/tp/doip/doip_client.c](src/tp/doip/doip_client.c).

## DoIP Client Structure

`DoIPClient_t` holds TCP/UDP transports, state, addresses, buffers, and flags. Relevant fields:

- `tcp`: DoIP TCP transport for diagnostics
- `udp`: DoIP UDP transport for discovery
- `source_address`, `target_address`: logical addresses
- `server_ip`, `server_port`: selected server endpoint
- `uds_response`/`uds_response_len`: buffered diagnostic response

See [src/tp/doip/doip_client.h](src/tp/doip/doip_client.h).

## Transport Layer (DoIP)

The DoIP transport abstraction is defined in [src/tp/doip/doip_transport.h](src/tp/doip/doip_transport.h).

- `DoIPTransport`:
  - `fd`, `port`, `ip`, `is_udp`, `loopback`
  - `connect_timeout_ms`, `send_timeout_ms`
- TCP helpers:
  - `doip_tp_tcp_init()`, `doip_tp_tcp_connect()`, `doip_tp_tcp_send()`, `doip_tp_tcp_recv()`, `doip_tp_tcp_close()`
- UDP helpers:
  - `doip_tp_udp_init()`, `doip_tp_udp_join_default_multicast()`,
    `doip_tp_udp_recv()`, `doip_tp_udp_recvfrom()`, `doip_tp_udp_sendto()`, `doip_tp_udp_close()`

### Non-Blocking Behavior

- Sockets are set to non-blocking (`O_NONBLOCK`).
- `select()` is used for readiness and timeouts.
- TCP connect: non-blocking with `EINPROGRESS` handling (select on writable + `SO_ERROR`). See [src/tp/doip/doip_tp_tcp.c](src/tp/doip/doip_tp_tcp.c).
- TCP send: send-all loop with timeout (`MSG_NOSIGNAL` used when available). Partial writes are handled until the buffer is sent or timeout. See [src/tp/doip/doip_tp_tcp.c](src/tp/doip/doip_tp_tcp.c).
- TCP/UDP recv: `select()` for readability then a single `recv()`/`recvfrom()`.

### Ports and Constants

Defined in [src/tp/doip/doip_defines.h](src/tp/doip/doip_defines.h):

- `DOIP_TCP_PORT` = 13400
- `DOIP_UDP_DISCOVERY_PORT` = 13400 (vehicle side)
- `DOIP_UDP_TEST_EQUIPMENT_REQUEST_PORT` = 13401 (tester side)
- `DOIP_DEFAULT_TIMEOUT_MS` = 5000

UDP discovery binds to 13401 by default (tester request port). Active VI requests are sent to 13400.

### Configuring Timeouts

```c
DoIPTransport *tcp = &tp.tcp;
// Override defaults per transport
doip_tp_set_timeouts(tcp, /*connect_timeout_ms=*/2000, /*send_timeout_ms=*/1000);
```

## UDP Discovery Details

Implemented in [src/tp/doip/doip_client.c](src/tp/doip/doip_client.c) and [src/tp/doip/doip_tp_udp.c](src/tp/doip/doip_tp_udp.c):

- Bind (loopback): `127.0.0.1:13401` to receive announcements/responses.
- Bind (non-loopback): `INADDR_ANY:13401` with `SO_BROADCAST` enabled.
- Active VI Request: `doip_tp_udp_sendto()` sends a header-only VI request (payload type 0x0001, length 0) to `13400`.
- Optional multicast join (non-loopback): `224.224.224.224` when not in request-only mode.
- VIN/EID/GID parsing: best-effort extraction from vehicle identification payloads.

## Example: Discovery CLI

See the example in [examples/doip_discovery_example/doip_discover.c](examples/doip_discovery_example/doip_discover.c):

- Loopback discover first responder:
```bash
(cd examples/doip_discovery_example && ./doip_discovery loopback)
```
- Filter by VIN prefix:
```bash
(cd examples/doip_discovery_example && ./doip_discovery WVWZZZ loopback)
```
- Options:
  - `--request-only`: send VI requests, skip multicast
  - `--raw`: dump raw frames via logger

## Configuration {#doip_configuration}

Compile-time and runtime:

- `DOIP_DEFAULT_TIMEOUT_MS` (compile-time): select timeouts default
- `doip_tp_set_timeouts()` (runtime): per-transport connect/send timeouts (TCP)
- `UDSDoIPSetDiscoveryOptions(request_only, dump_raw)` (runtime): discovery behavior

## See Also

- DoIP client implementation: [src/tp/doip/doip_client.c](src/tp/doip/doip_client.c)
- DoIP transport API: [src/tp/doip/doip_transport.h](src/tp/doip/doip_transport.h)
- TCP transport: [src/tp/doip/doip_tp_tcp.c](src/tp/doip/doip_tp_tcp.c)
- UDP transport: [src/tp/doip/doip_tp_udp.c](src/tp/doip/doip_tp_udp.c)
- DoIP constants: [src/tp/doip/doip_defines.h](src/tp/doip/doip_defines.h)
- Generic client guide: [docs/client.md](client.md)
