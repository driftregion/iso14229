# UDS firmware update based on [MCUBoot](https://docs.mcuboot.com/) and [Zephyr](https://www.zephyrproject.org/)

This is a UDSonCAN-based firmware update example, tested on the NUCLEO-G474RE development board.
It features a cryptographically signed application and a SecurityAccess implementation based on [1].

## Demo

This is an annotated and decoded `tshark` packet capture showing the firmware update sequence. 
Requests are sent by the client ([`fwupdate.py`](./fwupdate.py)), and responses are sent by the server in [`src/main.c`](./src/main.c) which runs on the NUCLEO-G474RE.

```sh
tshark -l -C uds -i can0 -d iso15765.subdissector,uds  # packet capture on socketcan can0 and UDS protocol decoding 

# ... `python fwupdate.py` invoked in another shell ...
    1 0.000000000              →              UDS 16 Request   Diagnostic Session Control             Extended Diagnostic Session
    2 0.002023247              →              UDS 16 Reply     Diagnostic Session Control             Extended Diagnostic Session   P2-default:  150ms  P2-enhanced:  1500ms
    3 0.004101887              →              UDS 16 Request   Control DTC Setting                    Off
    4 0.053852014              →              UDS 16 Reply     Control DTC Setting                    Off
    5 0.054885423              →              UDS 16 Request   Communication Control                  SubFunction: Enable RX and Disable TX
    6 0.105698753              →              UDS 16 Reply     Communication Control                  SubFunction: Enable RX and Disable TX
    7 0.106598281              →              UDS 16 Request   Diagnostic Session Control             Programming Session
    8 0.156492327              →              UDS 16 Reply     Diagnostic Session Control             Programming Session   P2-default: 1000ms  P2-enhanced:  5000ms
    9 0.157320759              →              UDS 16 Request   Security Access                        Request Seed (0x01)
   10 0.207296607              →              UDS 16 Reply     Security Access                        Request Seed (0x01)   dc 63 ed 8f
   11 0.212280748              →              ISO15765 16 First Frame(Frame Len: 258)   27 02 20 90 31 e7

# ... client gets seed from ECU ...
   51 0.232665211              →              ISO15765 16 Consecutive Frame(Seq: 3)   ba f9 4a 9f 1f 29 f8
   52 0.233113494              →              UDS 16 Request   Security Access                        Send Key (0x02)   20 90 31 e7 96 bd 8e 35 5e a6 a0 c1 89 da ef 4b 6c 16 ba 41 d7 7e 4b 3d …
   53 0.263960213              →              UDS 16 Reply     Security Access                        Send Key (0x02)
   54 0.264925886              →              UDS 16 Request   Request Download                       0xdd78 bytes at 0x0   (Compression:0x0 Encrypting:0x0)
   55 0.315812580              →              UDS 16 Reply     Request Download                       Max Block Length 0x100

# ... client downloads firmware to ECU with Transfer Data ...
 9654 11.857824833              →              UDS 16 Request   Transfer Data                          Block Sequence Counter 224   66 0f c4 46 37 0e 1b 6a d3 89 f8 be 1b 00 27 a2 ac 3d 62 6e b3 11 2f 6c …
 9655 11.903375478              →              UDS 16 Reply     Transfer Data                          Block Sequence Counter 224
 9656 11.904239332              →              UDS 16 Request   Request Transfer Exit               
 9657 11.953796023              →              UDS 16 Reply     Request Transfer Exit               
 9658 11.955360673              →              UDS 16 Request   Routine Control                        Start routine 0xff01 (checkProgrammingDependencies)
 9659 12.004868082              →              UDS 16 Reply     Routine Control                        Start routine 0xff01 (checkProgrammingDependencies)
 9660 12.006578351              →              UDS 16 Request   ECU Reset                              Hard Reset
 9661 12.056730119              →              UDS 16 Reply     ECU Reset                              Hard Reset

# ... server boots into new application ...
```

## Building and Flashing

The combined bootloader+app image is made with [sysbuild](https://docs.zephyrproject.org/latest/build/sysbuild/index.html). This combined image gets flashed once with a debug probe.
Subsequent programming is done over UDSonCAN, using [`fwupdate.py`](./fwupdate.py).

```sh
# build the bootloader and application for the first time
west build -b nucleo_g474re --sysbuild -d build/nucleo
west flash --runner pyocd -d build/nucleo

# subsequent building and flashing 
west build -b nucleo_g474re -d build/nucleo

pip install python-can can-isotp udsoncan cryptography
python fwupdate.py
```

## Keys

Two key pairs are used in this example: the image signing key ([`image_signing.pem`](./image_signing.pem)),
and the security access key ([`security_acccess.pem`](./security_access.pem)).
The idea here is that the signing key is airgapped at the firmware producer, and the bootloader will only run signed firmware. 
Production firmware updates get signed at a ceremony and then distributed.

In contrast, the security access key (private) lives inside programming tools which get distributed. Ideally this key is held in a hardware security module as proposed by [1].

Unsigned firmware *can* be loaded by a programming tool with the security access key, but that firmware will not boot. For example, the boot log below shows the result of flashing unsigned firmware with the following command: 
```sh
# flash **unsigned** firmware
python fwupdate.py build/nucleo/fwupdate/zephyr/zephyr.bin
```
After reboot, the bootloader output shows that the second image magic is unset, indicating that the integrity check has failed for the newly loaded second image.
```sh
I: Primary image: magic=good, swap_type=0x2, copy_done=0x1, image_ok=0x1
I: Secondary image: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3
```

### Generating Keys
The image signing key is generated with: 
```sh
openssl genrsa -out image_signing.pem 2048
```
and the security access key with the same command. 
```sh
openssl genrsa -out security_access.pem 2048
# public key
openssl rsa -in security_access.pem -RSAPublicKey_out -outform DER -out public_key.der
# C array -> pasted into main.c
python -c 'x=open("public_key.der", "rb").read(); print(", ".join([f"0x{i:02x}" for i in x]))'
```
The security access public key is stored inside the firmware image, and used during the UDS Security Access key validation phase.

## Disclaimer

- Generate your own keys. Do not use `image_signing.pem` and `security_access.pem` included in this example.

## Reference

[1] M. Thompson, “UDS Security Access for Constrained ECUs,” presented at the WCX SAE World Congress Experience, Detroit & Online, Michigan, United States, Mar. 2022, pp. 2022-01–0132. doi: 10.4271/2022-01-0132.