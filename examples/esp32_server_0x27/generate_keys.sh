#!/bin/bash

# Generate RSA key pair for testing the ESP32 server

echo "Generating RSA private key..."
openssl genrsa -out private_key.pem 4096

echo "Extracting public key..."
openssl rsa -in private_key.pem -pubout -outform PEM -out public_key.pem

echo "Generated keys:"
echo "  private_key.pem - Use this with UDS clients"
echo "  public_key.pem  - Copy this content to replace the embedded key in main.c"

echo ""
echo "To update the embedded public key in main.c:"
echo "1. Copy the content of public_key.pem"
echo "2. Replace the public_key_pem[] array in main.c"
echo "3. Rebuild and flash the ESP32"

echo ""
echo "Public key content:"
cat public_key.pem