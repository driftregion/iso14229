# Zephyr Server Example

UDS server as a [Zephyr freestanding application](https://docs.zephyrproject.org/latest/develop/application/index.html#zephyr-freestanding-application).

This example uses Zephyr's native CAN + ISO-TP stack (`CONFIG_CAN`, `CONFIG_ISOTP`).

Tested on:
- `nucleo_g474re` (FDCAN1, pins PB8/PB9, 500 kbaud — see `boards/nucleo_g474re.overlay`)
- `native_sim` (connected to host `vcan` interface on linux)

Physical addressing: request `0x7E0` / response `0x7E8`. Functional (broadcast) request: `0x7DF`.

# Building

```sh
source ~/zephyrproject/.venv/bin/activate
source ~/zephyrproject/zephyr/zephyr-env.sh
```

## Nucleo G474RE
```sh
west build -b nucleo_g474re -d build/nucleo
west flash --runner pyocd
```

## `native_sim`

`native_sim` defaults to using the linux virtual socketcan interface `vcan0`.
Enable it on the host with:
```sh
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

```sh
west build -b native_sim -d build/native
./build/native/zephyr/zephyr.exe -rt
```

then, on the host,
```sh
cansend vcan0 7DF#023E000000000000
```
and in a separate shell,
```sh
candump vcan0
  vcan0  7DF   [8]  02 3E 00 00 00 00 00 00  # <-- message from host
  vcan0  7E8   [3]  02 7E 00                 # <-- response from server
```
