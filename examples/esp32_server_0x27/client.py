#!/usr/bin/env python3
"""
Python UDS Client for ESP32 Server with Security Access (0x27)

This client demonstrates how to perform Security Access authentication
with the ESP32 UDS server using RSA signatures.

Requirements:
- udsoncan
- python-can
- cryptography
- A CAN interface (SocketCAN on Linux)

Usage:
    python client.py --interface vcan0 --private-key private_key.pem
"""

import argparse
import logging
import time
from typing import Optional

import can
from udsoncan import IsoTpMessage
from udsoncan.client import Client
from udsoncan.common.services import SecurityAccess, WriteDataByIdentifier
from udsoncan.exceptions import NegativeResponseException, TimeoutException
from udsoncan.connections import IsoTpConnection

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa


class ESP32UDSClient:
    """UDS Client for ESP32 server with Security Access support."""

    def __init__(self, can_interface: str, private_key_path: str):
        self.can_interface = can_interface
        self.private_key_path = private_key_path
        self.private_key: Optional[rsa.RSAPrivateKey] = None
        self.client: Optional[Client] = None

        # UDS addressing
        self.client_id = 0x7E0  # Client sends to server
        self.server_id = 0x7E8  # Server responds from

        self.logger = logging.getLogger(__name__)

    def load_private_key(self):
        """Load RSA private key from PEM file."""
        try:
            with open(self.private_key_path, 'rb') as key_file:
                self.private_key = serialization.load_pem_private_key(
                    key_file.read(),
                    password=None
                )
            self.logger.info(f"Loaded private key from {self.private_key_path}")
        except Exception as e:
            raise RuntimeError(f"Failed to load private key: {e}")

    def connect(self):
        """Connect to CAN bus and initialize UDS client."""
        try:
            # Create CAN bus connection
            bus = can.interface.Bus(channel=self.can_interface, bustype='socketcan')

            # Create ISO-TP connection
            tp_addr = IsoTpMessage(
                arbitration_id=self.client_id,
                data=None,
                extended_id=False
            )

            connection = IsoTpConnection(
                bus=bus,
                arbitration_id=self.client_id,
                address_extension=None
            )

            # Create UDS client
            self.client = Client(connection)

            self.logger.info(f"Connected to CAN interface: {self.can_interface}")

        except Exception as e:
            raise RuntimeError(f"Failed to connect to CAN: {e}")

    def sign_seed(self, seed: bytes) -> bytes:
        """Sign the seed with RSA private key using SHA-256."""
        if not self.private_key:
            raise RuntimeError("Private key not loaded")

        try:
            signature = self.private_key.sign(
                seed,
                padding.PKCS1v15(),
                hashes.SHA256()
            )
            self.logger.info(f"Signed {len(seed)}-byte seed, signature length: {len(signature)}")
            return signature
        except Exception as e:
            raise RuntimeError(f"Failed to sign seed: {e}")

    def perform_security_access(self, level: int = 1) -> bool:
        """
        Perform Security Access authentication.

        Args:
            level: Security level to unlock (1 = request seed, 2 = send key)

        Returns:
            True if authentication successful, False otherwise
        """
        if not self.client:
            raise RuntimeError("Client not connected")

        try:
            self.logger.info(f"Starting Security Access for level {level}")

            # Step 1: Request seed
            self.logger.info("Requesting seed...")
            response = self.client.send_request(
                SecurityAccess.RequestSeed(level)
            )

            if response.service != SecurityAccess.RequestSeed:
                self.logger.error("Unexpected response to seed request")
                return False

            seed = response.data
            self.logger.info(f"Received {len(seed)}-byte seed: {seed.hex()}")

            # Step 2: Sign the seed
            signature = self.sign_seed(seed)

            # Step 3: Send signed key
            self.logger.info("Sending signed key...")
            response = self.client.send_request(
                SecurityAccess.SendKey(level + 1, signature)
            )

            if response.service != SecurityAccess.SendKey:
                self.logger.error("Unexpected response to key submission")
                return False

            self.logger.info("Security Access successful!")
            return True

        except NegativeResponseException as e:
            self.logger.error(f"Security Access failed: {e}")
            return False
        except TimeoutException:
            self.logger.error("Security Access timed out")
            return False
        except Exception as e:
            self.logger.error(f"Security Access error: {e}")
            return False

    def write_data_by_identifier(self, data_id: int, data: bytes) -> bool:
        """
        Write data using UDS service 0x2E.

        Args:
            data_id: Data identifier
            data: Data to write

        Returns:
            True if successful, False otherwise
        """
        if not self.client:
            raise RuntimeError("Client not connected")

        try:
            self.logger.info(f"Writing data 0x{data_id:04X}: {data.hex()}")

            response = self.client.send_request(
                WriteDataByIdentifier(data_id, data)
            )

            if response.service != WriteDataByIdentifier:
                self.logger.error("Unexpected response to write data")
                return False

            self.logger.info("Write data successful!")
            return True

        except NegativeResponseException as e:
            self.logger.error(f"Write data failed: {e}")
            return False
        except TimeoutException:
            self.logger.error("Write data timed out")
            return False
        except Exception as e:
            self.logger.error(f"Write data error: {e}")
            return False

    def test_led_control(self):
        """Test LED control after successful security access."""
        self.logger.info("Testing LED control...")

        # LED patterns to test
        patterns = [
            (0x01, "Red LED ON"),
            (0x02, "Green LED ON"),
            (0x04, "Blue LED ON"),
            (0x07, "All LEDs ON"),
            (0x00, "All LEDs OFF")
        ]

        for pattern, description in patterns:
            self.logger.info(f"Setting: {description}")
            if self.write_data_by_identifier(0x0001, bytes([pattern])):
                time.sleep(1)  # Visual delay
            else:
                self.logger.error(f"Failed to set pattern: {description}")
                break

    def run_demo(self):
        """Run complete demonstration."""
        try:
            self.logger.info("Starting ESP32 UDS Client Demo")

            # Load cryptographic key
            self.load_private_key()

            # Connect to CAN
            self.connect()

            # Perform security access
            if not self.perform_security_access(level=1):
                self.logger.error("Security access failed, cannot continue")
                return False

            # Test LED control (requires security access)
            self.test_led_control()

            self.logger.info("Demo completed successfully!")
            return True

        except Exception as e:
            self.logger.error(f"Demo failed: {e}")
            return False
        finally:
            if self.client:
                self.client.close()


def setup_logging(verbose: bool = False):
    """Setup logging configuration."""
    level = logging.DEBUG if verbose else logging.INFO
    format_str = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'

    logging.basicConfig(
        level=level,
        format=format_str,
        handlers=[
            logging.StreamHandler(),
            logging.FileHandler('uds_client.log')
        ]
    )


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='UDS Client for ESP32 Server with Security Access'
    )
    parser.add_argument(
        '--interface', '-i',
        default='vcan0',
        help='CAN interface name (default: vcan0)'
    )
    parser.add_argument(
        '--private-key', '-k',
        default='private_key.pem',
        help='Path to RSA private key file (default: private_key.pem)'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose logging'
    )

    args = parser.parse_args()

    # Setup logging
    setup_logging(args.verbose)

    # Create and run client
    client = ESP32UDSClient(args.interface, args.private_key)

    try:
        success = client.run_demo()
        exit_code = 0 if success else 1
        exit(exit_code)
    except KeyboardInterrupt:
        logging.info("Interrupted by user")
        exit(1)
    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        exit(1)


if __name__ == '__main__':
    main()