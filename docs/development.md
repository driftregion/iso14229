## Development

Welcome aboard. You will need linux and the build system bazel. 

Use the pre-commit hook in .githooks to automate formatting the sources and updating the amalagamation. Configure them with:
```sh
git config core.hooksPath .githooks
```

## Running Tests

```sh
bazel test //...
```

See [test/README.md](test/README.md)

## CAN Log Decoding

The examples use (`cansend` `candump`) to send and display CAN frames. 
For example, this command sends "Tester Present" to 0x7E0.
```sh
cansend vcan0 7e0#023e000000000000
```
and `candump` shows the sent command and the response. 
```sh
candump vcan0
vcan0  7E0   [8]  02 3E 00 00 00 00 00 00
vcan0  7E8   [3]  02 7E 00
```
However, this level of abstraction is too low to show UDS service requests and responses clearly, especially those which span multiple CAN frames.

cantools also includes `isotpsend` and `isotprecv`. The interface is different from `cansend` and `candump`.
```sh
isotprecv -l -s 7e8 -d 7e0 vcan0 
22 F1 00
```
```sh
 echo "22 F1 00" | isotpsend -s 7e0 -d 7e8 vcan0
```

`tshark` can decode ISO-TP messages and UDS services:
```sh
tshark -l -C uds -i vcan0 -d iso15765.subdissector,uds
Capturing on 'vcan0'
    1 0.000000000              →              UDS 16 Request   Tester Present                         SubFunction: 0x00
    2 0.055207327              →              UDS 16 Reply     Tester Present                         SubFunction: 0x00
```

`tshark` is powerful. The options for configuring ISO-TP and UDS decoding are numerous,
including RDBI/WDBI DID lookups and more.

I made a wireshark profile called `uds`.
This profile is a directory (e.g. `~/.config/wireshark/profiles/uds`) containing configuration files.  
see: https://tshark.dev/packetcraft/arcana/profiles/

`preferences`:
```txt
iso15765.can.ids: 0x7df-0x7ef
```
