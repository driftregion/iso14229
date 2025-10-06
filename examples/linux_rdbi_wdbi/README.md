# Read/Write Data By Identifier Example

UDS client and server demonstrating Read Data By Identifier (0x22) and Write Data By Identifier (0x2E) services.

## Overview

This example shows how to implement RDBI/WDBI services for reading and writing data identifiers on a UDS server, along with a corresponding client that exercises these services.

## Files

- \ref examples/linux_rdbi_wdbi/server.c - Server implementation with RDBI/WDBI handlers
- \ref examples/linux_rdbi_wdbi/client.c - Client implementation demonstrating RDBI/WDBI requests

## Features Demonstrated

- Reading data identifiers (0x22)
- Writing data identifiers (0x2E)
- Data identifier handlers
- Client request sequences

## Building

See the main repository README for build instructions.

## Running

Requires a CAN interface (virtual or physical) with ISO-TP support.

```bash
# Terminal 1: Run server
./linux_rdbi_wdbi_server vcan0

# Terminal 2: Run client
./linux_rdbi_wdbi_client vcan0
```
