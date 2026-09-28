#!/bin/bash

if ip link show vcan0 > /dev/null 2>&1; then
    echo "vcan0 is already up"
    exit 0
fi

sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up