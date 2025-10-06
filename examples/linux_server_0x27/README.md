# Security Access Example (0x27)

UDS client and server demonstrating Security Access (0x27) service with cryptographic challenge-response.

## Overview

This example shows how to implement secure authentication using the Security Access service, based on cryptographic seed-key exchange.

## Files

- \ref examples/linux_server_0x27/server.c - Server implementation with Security Access handler
- \ref examples/linux_server_0x27/client.c - Client implementation demonstrating Security Access requests

## Running the example

```sh
apt install libmbedtls-dev
sudo ip link add name vcan0 type vcan
sudo ip link set vcan0 up

make

./server

./client
```

## Acknowledgement

This example is based on Martin Thompson's paper "UDS Security Access for Constrained ECUs" https://doi.org/10.4271/2022-01-0132
