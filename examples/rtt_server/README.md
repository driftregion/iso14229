# UDS Server Example for RT-Thread

UDS server example for RT-Thread platform.

## Overview

This example demonstrates a UDS (ISO 14229) server running on an RT-Thread based embedded system. It uses the RT-Thread CAN device driver for communication.

Also included is a Linux-based client example that can be run from a host computer to test the server. This setup allows for diagnostics and interaction with the embedded device over a CAN bus.

## Required Hardware

#### Server (Embedded Device)
*   Any development board running the RT-Thread operating system.
*   A CAN transceiver connected to the board's CAN peripheral (e.g., a board with an integrated SN65HVD230).

#### Client (Host Machine)
*   A Linux computer (or a virtual machine).
*   A SocketCAN-compatible adapter (e.g., USB-to-CAN adapter) connected to the host and the embedded device's CAN bus.

## Setup: Server (RT-Thread Device)

1.  **Integrate the Library**: Add the UDS library source code to your RT-Thread project.
2.  **Configure the Port**: Ensure the `rtt_uds_example.c` file is correctly configured for your target board, especially the CAN device name (`UDS_EXAMPLE_CAN_DEVICE_NAME`) and any GPIOs used in the example.
3.  **Compile and Flash**: Build and flash the RT-Thread project to your target hardware using your standard toolchain (e.g., SCons).
4.  **Run the Example**: Connect to the device's serial console (e.g., via MSH/Finsh) and start the UDS server example.

```sh
# Connect to the RT-Thread serial console
msh />
msh />uds_example start
11-14 19:53:09 isotp.rtt: UDS example started on can1.
```

## Setup: Client (Linux Host)

1.  **Configure SocketCAN**: Bring up the CAN interface on your Linux machine.

    ```sh
    # Example for a device at 1Mbit/s
    sudo ip link set can0 up type can bitrate 1000000
    ```

2.  **Compile and Run**: Navigate to the client example directory, compile it, and run the executable.

    ```sh
    # Assuming the client source files are in the current directory
    make && ./client
    ```

## Example Interaction Log

This section shows a typical interaction between the Linux client and the RT-Thread server, along with the corresponding CAN traffic dump.

### Server Output (RT-Thread MSH/Finsh Console)

After starting the server with `uds_example start`, the device begins listening for CAN messages. When the client sends `WriteDataByIdentifier` requests, the server processes them and prints log messages.

```sh
msh />uds_example start
11-14 19:53:09 isotp.rtt: UDS example started on can1.

# Client sends a request to write 0x01 to DID 0x0001
11-14 19:53:10 isotp.rtt: CAN RX ID:0x7E0 [5 bytes]: 04 2E 00 01 01
11-14 19:53:10 UDS.core: I (6703) ./iso14229.c: phys link received 4 bytes
11-14 19:53:10 isotp.rtt: Server Event: UDS_EVT_WriteDataByIdent (0xA)
11-14 19:53:10 isotp.rtt: --> WDBI DID:0x0001 Data [1 bytes]: 01
11-14 19:53:10 isotp.rtt: Controlling LEDs with value: 0x01
11-14 19:53:10 isotp.rtt: [TX] ID: 0x7E8 [4 bytes]: 03 6E 00 01

# Client sends a request to write 0x02 to DID 0x0001
11-14 19:53:10 isotp.rtt: CAN RX ID:0x7E0 [5 bytes]: 04 2E 00 01 02
11-14 19:53:10 UDS.core: I (6762) ./iso14229.c: phys link received 4 bytes
11-14 19:53:10 isotp.rtt: Server Event: UDS_EVT_WriteDataByIdent (0xA)
11-14 19:53:10 isotp.rtt: --> WDBI DID:0x0001 Data [1 bytes]: 02
11-14 19:53:10 isotp.rtt: Controlling LEDs with value: 0x02
11-14 19:53:10 isotp.rtt: [TX] ID: 0x7E8 [4 bytes]: 03 6E 00 01
# ... and so on ...
```

### Client Output (Linux Console)

The client application shows the state transitions and events as it sends requests and receives responses from the server.

```sh
root@root:/Debug# ./client 
I (2431038538) src/tp/isotp_sock.c: client initialized phys link (fd 3) rx 0x7e8 tx 0x7e0 func link (fd 4) rx 0x7e8 tx 0x7df
I (2431038538) client.c: polling
I (2431038538) src/client.c: client state: Idle (0) -> Sending (1)
I (2431038538) src/client.c: client state: Sending (1) -> AwaitSendComplete (2)
I (2431038538) client.c: UDS_EVT_SendComplete (26)
I (2431038538) src/client.c: client state: AwaitSendComplete (2) -> AwaitResponse (3)
I (2431038559) src/client.c: received 3 bytes. Processing...
I (2431038559) client.c: UDS_EVT_ResponseReceived (27)
I (2431038559) client.c: WDBI response received
I (2431038559) src/client.c: client state: AwaitResponse (3) -> Idle (0)
I (2431038559) client.c: UDS_EVT_Idle (28)
# ... loop continues, sending next request ...
```

### CAN Bus Traffic (`candump`)

Using the `candump` utility on the Linux host shows the raw CAN frames exchanged between the client and the server.

*   `7E0`: Client (tester) sending a request to the server (ECU).
*   `7E8`: Server (ECU) sending a response back to the client.

```sh
candump can0
  # Request: Write Data (0x2E) to DID 0x0001 with value 0x01
  can0  7E0   [5]  04 2E 00 01 01
  # Response: Positive Response (0x6E) for service 0x2E, DID 0x0001
  can0  7E8   [4]  03 6E 00 01
  
  # Request: Write Data (0x2E) to DID 0x0001 with value 0x02
  can0  7E0   [5]  04 2E 00 01 02
  # Response: Positive Response
  can0  7E8   [4]  03 6E 00 01

  # Request: Write Data (0x2E) to DID 0x0001 with value 0x03
  can0  7E0   [5]  04 2E 00 01 03
  # Response: Positive Response
  can0  7E8   [4]  03 6E 00 01
# ... and so on ...
```

### **⚠️ Important Note**

The code and installation instructions here are merely presented as a simple example to demonstrate the basic integration and functionality. For the complete application and detailed functional code, please refer to the code repository:
**[iso14229 rtt software](https://github.com/wdfk-prog/can_uds)**
