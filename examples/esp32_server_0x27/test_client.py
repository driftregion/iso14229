#!/usr/bin/env python3
"""
Simple test script for the ESP32 UDS client.

This script provides a simplified interface to test specific features
of the ESP32 UDS server without running the full demo.
"""

import argparse
import logging
from client import ESP32UDSClient


def test_security_access_only(interface: str, private_key: str):
    """Test only the Security Access functionality."""
    print("Testing Security Access only...")

    client = ESP32UDSClient(interface, private_key)

    try:
        client.load_private_key()
        client.connect()

        success = client.perform_security_access(level=1)

        if success:
            print("✓ Security Access successful!")
        else:
            print("✗ Security Access failed!")

        return success

    except Exception as e:
        print(f"✗ Error: {e}")
        return False
    finally:
        if client.client:
            client.client.close()


def test_led_pattern(interface: str, private_key: str, pattern: int):
    """Test setting a specific LED pattern."""
    print(f"Testing LED pattern: 0x{pattern:02X}")

    client = ESP32UDSClient(interface, private_key)

    try:
        client.load_private_key()
        client.connect()

        # First authenticate
        if not client.perform_security_access(level=1):
            print("✗ Security Access failed!")
            return False

        # Set LED pattern
        success = client.write_data_by_identifier(0x0001, bytes([pattern]))

        if success:
            print(f"✓ LED pattern 0x{pattern:02X} set successfully!")
        else:
            print(f"✗ Failed to set LED pattern 0x{pattern:02X}!")

        return success

    except Exception as e:
        print(f"✗ Error: {e}")
        return False
    finally:
        if client.client:
            client.client.close()


def main():
    """Main test interface."""
    parser = argparse.ArgumentParser(description='ESP32 UDS Client Test Utilities')
    parser.add_argument('--interface', '-i', default='vcan0', help='CAN interface')
    parser.add_argument('--private-key', '-k', default='private_key.pem', help='Private key file')

    subparsers = parser.add_subparsers(dest='command', help='Test commands')

    # Security access test
    subparsers.add_parser('auth', help='Test Security Access only')

    # LED pattern test
    led_parser = subparsers.add_parser('led', help='Test LED pattern')
    led_parser.add_argument('pattern', type=lambda x: int(x, 0), help='LED pattern (hex or decimal)')

    # Full demo
    subparsers.add_parser('demo', help='Run full demo')

    args = parser.parse_args()

    # Suppress verbose logging for test utilities
    logging.getLogger().setLevel(logging.WARNING)

    if args.command == 'auth':
        test_security_access_only(args.interface, args.private_key)
    elif args.command == 'led':
        test_led_pattern(args.interface, args.private_key, args.pattern)
    elif args.command == 'demo':
        client = ESP32UDSClient(args.interface, args.private_key)
        client.run_demo()
    else:
        parser.print_help()


if __name__ == '__main__':
    main()