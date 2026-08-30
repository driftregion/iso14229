# Security Access Example (0x27)

UDS client and server demonstrating Security Access (0x27) service with cryptographic challenge-response.

This example shows how to implement secure authentication using the Security Access service, based on cryptographic seed-key exchange.

## Running the example

```sh
sudo apt install libmbedtls-dev

sudo ip link add name vcan0 type vcan
sudo ip link set vcan0 up

# build server and client, and generate a key pair
make

# run the server in one terminal
./server

# and the client in another
./client
```

Server output:
```sh
./server
I (1360185151) src/tp/isotp_sock.c: server initialized phys link rx 0x7e0 tx 0x7e8 func link rx 0x7df tx 0x7e8
I (1360185151) server.c: server up, polling . . .
I (1360188174) server.c: Server event: UDS_EVT_SecAccessRequestSeed (8)
I (1360188174) server.c: Generating seed for level 3
I (1360188419) server.c: Server event: UDS_EVT_SecAccessValidateKey (9)
I (1360188419) server.c: Validating key, level=3, len=512
I (1360188420) server.c: Security level 3 unlocked
```

Client output (I ran it twice):
```
./client
I (1360188173) src/tp/isotp_sock.c: client initialized phys link (fd 3) rx 0x7e8 tx 0x7e0 func link (fd 4) rx 0x7e8 tx 0x7df
I (1360188173) client.c: polling
I (1360188184) client.c: got seed: 
91 F8 D7 41 14 00 6D 3E EB A5 45 CA F0 39 FB 03 9E 93 EF EA C5 0F 3F 0B 9D 43 AF DF C9 4C A7 F4 
I (1360188184) client.c: Signing key...
I (1360188211) client.c: Requesting Security Access...
I (1360188421) client.c: Security Access Obtained

./client
I (1360193207) src/tp/isotp_sock.c: client initialized phys link (fd 3) rx 0x7e8 tx 0x7e0 func link (fd 4) rx 0x7e8 tx 0x7df
I (1360193207) client.c: polling
I (1360193209) client.c: got seed: 
00 00 
I (1360193209) client.c: seed is all zero, already unlocked
```

## Acknowledgement

This example is based on Martin Thompson's paper "UDS Security Access for Constrained ECUs" https://doi.org/10.4271/2022-01-0132
