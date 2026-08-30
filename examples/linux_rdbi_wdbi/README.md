# Read/Write Data By Identifier Example

UDS client and server demonstrating Read Data By Identifier (0x22) and Write Data By Identifier (0x2E) services.

## Overview

This example shows how to implement RDBI/WDBI services for reading and writing data identifiers on a UDS [server](./server.c), along with a corresponding [client](./client.c) that exercises these services.


## Running

Requires a CAN interface (virtual or physical) with ISO-TP support.

```bash
# build 
make 

# Set up a virtual CAN interface
sudo ip link add dev vcan0 type vcan 
sudo ip link set vcan0 up

# Terminal 1: Run server
./server

# Terminal 2: Run client
./cllient
```

Server Output:
```sh
./server
I (1356965235) src/tp/isotp_sock.c: configuring fd: 4 as functional
I (1356965235) src/tp/isotp_sock.c: server initialized phys link rx 0x7e0 tx 0x7e8 func link rx 0x7df tx 0x7e8
I (1356965235) server.c: server up, polling . . .
I (1356969530) server.c: Received RDBI for data id: 0xF190
I (1356969532) server.c: Received WDBI for data id: 0xF190
I (1356969532) server.c: Wrote 1 to 0xF190
I (1356975299) server.c: Received RDBI for data id: 0xF190
I (1356975301) server.c: Received WDBI for data id: 0xF190
I (1356975301) server.c: Wrote 2 to 0xF190
```

Client Output (I ran it twice):
```sh
 ./client
I (1356969529) src/tp/isotp_sock.c: configuring fd: 4 as functional
I (1356969529) src/tp/isotp_sock.c: client initialized phys link (fd 3) rx 0x7e8 tx 0x7e0 func link (fd 4) rx 0x7e8 tx 0x7df
I (1356969529) client.c: polling
I (1356969529) client.c: Sending Read Data By Identifier (RDBI) for data identifier 0xf190
I (1356969531) client.c: Received Positive Response to RDBI. 0xf190 has value 0
I (1356969531) client.c: Sending Write Data By Identifier (WDBI) for data identifier 0xf190, writing value: 1
I (1356969582) client.c: Received Positive Response to WDBI. Exiting.
./client
I (1356975298) src/tp/isotp_sock.c: configuring fd: 4 as functional
I (1356975298) src/tp/isotp_sock.c: client initialized phys link (fd 3) rx 0x7e8 tx 0x7e0 func link (fd 4) rx 0x7e8 tx 0x7df
I (1356975298) client.c: polling
I (1356975298) client.c: Sending Read Data By Identifier (RDBI) for data identifier 0xf190
I (1356975300) client.c: Received Positive Response to RDBI. 0xf190 has value 1
I (1356975300) client.c: Sending Write Data By Identifier (WDBI) for data identifier 0xf190, writing value: 2
I (1356975351) client.c: Received Positive Response to WDBI. Exiting.
```
