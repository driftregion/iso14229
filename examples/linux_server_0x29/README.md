# Linux Server Authentication sample

This sample shows an example authentication workflow heavily inpired by Example 6 of the *ISO14229-1 2020* standard (chapter *10.6.8.6*).

The required AES Key is generated during build.


# Building the example

The sample uses mbedtls for key generation and challenge validation:

```sh
apt install libmbedtls-dev
```

Setup a virtual CAN bus

```sh
sudo ip link add name vcan0 type vcan
sudo ip link set vcan0 up
```

Build executables

```sh
make
```

# Running the example

Simply run the client and the server in two different terminals

```sh
./server

./client
```

