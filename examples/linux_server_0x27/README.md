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