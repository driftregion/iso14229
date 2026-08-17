# Arduino Server Example

UDS server example for Arduino platforms. 
This example demonstrates the ECUReset service.

## Software 
This example uses the  [`CAN` Arduino library](https://github.com/sandeepmistry/arduino-can) by Sandeep Mistry. 

## Hardware

The sketch should work out of the box on any Arduino with CAN support.
I used the following hardware:

- [Arduino MKR-WIFI 1010](https://store-usa.arduino.cc/products/arduino-mkr-wifi-1010)
- [MKR CAN Shield](https://store.arduino.cc/products/arduino-mkr-can-shield)

On my MKR CAN Shield, the labeling of CAN-H and CAN-L on silkscreen on the MKR CAN Shield was contradictory on the top and bottom of the board. The top is correct.

## Setup: Server 

Build and flash `main.ino` with the Arduino GUI or using the cli as follows:
```
# flash the program
arduino-cli -b arduino:samd:mkrwifi1010 compile examples/arduino_server/main -u -p /dev/ttyACM0
```

## Setup: Client

Build and run the client with
```sh
make && ./client
```

Running the client sends an ECU reset request.

## Example Output:

On the server:
```
Starting Arduino UDS Server
Arduino UDS Server: Setup Complete
Got event 2, (UDS_EVT_EcuReset)
EcuReset
Got event 20, (UDS_EVT_DoScheduledReset)
```

On the client:
```
./client
I (286928572) src/tp/isotp_sock.c: configuring fd: 4 as functional
I (286928572) src/tp/isotp_sock.c: client initialized phys link (fd 3) rx 0x7e8 tx 0x7e0 func link (fd 4) rx 0x7e8 tx 0x7df
I (286928572) client.c: polling
I (286928572) client.c: UDS_EVT_SendComplete (26)
I (286928573) client.c: UDS_EVT_ResponseReceived (27)
I (286928573) client.c: ECUReset response received
I (286928573) client.c: UDS_EVT_Idle (28)
```
