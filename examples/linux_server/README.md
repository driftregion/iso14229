# Linux Server Example

Basic UDS server example using Linux kernel ISO-TP sockets (socketcan).

## Overview

This example demonstrates a minimal UDS server running on Linux using the kernel's built-in ISO-TP support via SocketCAN.

## Files

- \ref examples/linux_server/main.c - Main server implementation

## Building

See the main repository README for build instructions.

## Running

Requires a CAN interface (virtual or physical) with ISO-TP support.

```bash
# Create virtual CAN interface
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Run the server
./linux_server vcan0
```

## Requirements

- Linux kernel with ISO-TP support (CONFIG_CAN_ISOTP)
- SocketCAN interface
