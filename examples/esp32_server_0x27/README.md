# ESP32 UDS Server with Security Access (0x27) Example

This example demonstrates a UDS server running on ESP32 with Security Access (0x27) service support using RSA signature verification.

## Required Hardware

- [ESP32-C3-32S](https://docs.ai-thinker.com/_media/esp32/docs/esp-c3-32s-kit-v1.0_specification.pdf)
- [Waveshare SN65HVD230 CAN Board](https://www.waveshare.com/sn65hvd230-can-board.htm)

## Hardware Connections

| ESP32-C3 Pin | CAN Board Pin | Function |
|--------------|---------------|----------|
| GPIO6        | CTX           | CAN TX   |
| GPIO7        | CRX           | CAN RX   |
| 3V3          | 3V3           | Power    |
| GND          | GND           | Ground   |

### LED Connections (Optional)

| ESP32-C3 Pin | LED Color | Function |
|--------------|-----------|----------|
| GPIO3        | Red       | Error/Security Denied |
| GPIO4        | Green     | Success/Security Unlocked |
| GPIO5        | Blue      | General Status |

## Setup

1. Install ESP-IDF development framework
2. Connect CAN board to ESP32 as shown above
3. Copy `iso14229.c` and `iso14229.h` to the `main/` directory:
   ```bash
   cp ../../iso14229.c ../../iso14229.h main/
   ```

## Building and Flashing

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Configure the project (optional - defaults should work)
idf.py menuconfig

# Build the project
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor
```

## Features

- **Security Access (0x27)**: RSA signature-based authentication
- **Secure Random Seeds**: Uses ESP32 hardware random number generator
- **LED Indicators**: Visual feedback for security operations
- **Write Data By Identifier (0x2E)**: LED control via UDS
- **ISO-TP over CAN**: Standard automotive diagnostic communication

## Security Access Flow

1. Client requests seed with service 0x27 (sub-function 0x01)
2. Server generates secure random 32-byte seed and responds
3. Client signs the seed with RSA private key and sends signature with service 0x27 (sub-function 0x02)
4. Server verifies signature using embedded public key
5. If valid, security level is unlocked (green LED lights up)
6. If invalid, access is denied (red LED lights up)

## Testing

Use the linux_server_0x27 client example to test this server:

```bash
# On Linux machine with CAN interface
cd ../linux_server_0x27
make
./client
```

## Notes

- The public key is embedded in the source code for demonstration purposes
- In production, use secure key storage (ESP32 NVS encryption, secure element, etc.)
- The example uses a 4096-bit RSA key for high security
- Adjust CAN bit rate (500kbps default) as needed for your network

## Python Client Testing

A Python client is provided to test the ESP32 server:

### Setup Python Environment

```bash
# Install Python dependencies
pip install -r requirements.txt

# Generate RSA key pair for testing
./generate_keys.sh
```

### Running the Client

```bash
# Full demo (Security Access + LED control)
python client.py --interface vcan0 --private-key private_key.pem

# Test only Security Access
python test_client.py auth

# Test specific LED pattern (red LED)
python test_client.py led 0x01

# Test all LEDs
python test_client.py led 0x07
```

### Client Features

- **Security Access**: RSA signature-based authentication
- **LED Control**: Test Write Data By Identifier service
- **Comprehensive Logging**: Detailed operation logs
- **Error Handling**: Robust error detection and reporting

## Acknowledgement

This example is based on Martin Thompson's paper "UDS Security Access for Constraint ECUs" https://doi.org/10.4271/2022-01-0132