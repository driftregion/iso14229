# Running the example

```sh
apt install libmbedtls-dev
sudo ip link add name vcan0 type vcan
sudo ip link set vcan0 up

make

./server

./client
```

# Known issues

- auth only works once

# Todo

- implement multiple security levels

# Acknowledgement

This example is based on Martin Thompson's paper "UDS Security Access for Constraint ECUs" https://doi.org/10.4271/2022-01-0132